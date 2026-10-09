/// @file test_mcp_types.cpp
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

struct CurrentPathGuard {
    fs::path previous;

    explicit CurrentPathGuard(const fs::path& next) : previous(fs::current_path()) {
        fs::current_path(next);
    }

    ~CurrentPathGuard() {
        std::error_code ec;
        fs::current_path(previous, ec);
    }
};

[[nodiscard]] bool c13_ancestry_clean(const fs::path& base) {
    std::error_code ec;
    for (auto d = base; ; d = d.parent_path()) {
        if (fs::exists(d / ".git", ec)) return false;
        if (d.parent_path() == d || d.parent_path().empty()) return true;
    }
}

[[nodiscard]] std::optional<fs::path> c13_clean_temp_base() {
    std::error_code ec;
    for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
        if (const char* v = std::getenv(var);
            v != nullptr && fs::is_directory(v, ec) && !ec &&
            c13_ancestry_clean(v)) {
            return fs::path(v);
        }
        ec.clear();
    }
    if (fs::is_directory("/dev/shm", ec) && !ec &&
        c13_ancestry_clean("/dev/shm")) {
        return fs::path("/dev/shm");
    }
    if (c13_ancestry_clean(fs::temp_directory_path())) {
        return fs::temp_directory_path();
    }
    return std::nullopt;
}

[[nodiscard]] fs::path c13_make_temp_root(std::string_view name) {
    static std::atomic<unsigned> counter{0};
    const auto base = c13_clean_temp_base();
    const fs::path base_dir = base.value_or(fs::temp_directory_path());
    constexpr int kMaxAttempts = 64;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const fs::path root =
            base_dir /
            (std::string(name) + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1,
                 std::memory_order_relaxed)) + "_" +
             std::to_string(std::chrono::system_clock::now()
                                 .time_since_epoch().count()));
        std::error_code create_ec;
        fs::create_directories(root, create_ec);
        std::error_code probe_ec;
        if (fs::is_directory(root, probe_ec) && !probe_ec) {
            return root;  // created, or a unique name that now exists
        }
        // Collision / transient: take the next unique name.
    }
    // Exhausted (effectively impossible with unique names): return the
    // system temp dir so a failure surfaces as a clear test error rather
    // than a dangling empty path.
    return fs::temp_directory_path();
}

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

[[nodiscard]] std::string c6_read_file(const fs::path& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void c6_write_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
}

[[nodiscard]] std::size_t c6_count_occurrences(std::string_view haystack,
                                               std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

[[nodiscard]] std::vector<std::string>
c6_json_keys(loom::utils::json::JsonVal object) {
    std::vector<std::string> keys;
    object.iter_obj([&](auto key, auto) {
        if (key.is_str()) keys.emplace_back(key.as_str());
    });
    return keys;
}

} // namespace

TEST(McpConfigParser, ParsesJsonFieldsAndExplicitTransports) {
    const auto parsed = loom::services::mcp::ConfigParser::parse_json(R"JSON({
      "mcpServers": {
        "stdio_fixture": {
          "type": "stdio",
          "command": "node",
          "args": ["server.js", "--flag"],
          "env": {"FOO": "bar"},
          "timeout": 1234,
          "autoStart": false,
          "enabled": false
        },
	"sse_fixture": {
	  "type": "sse",
	  "url": "http://127.0.0.1:8123/events",
	  "headers": {"Authorization": "Bearer token"},
	  "headersHelper": "node helper.js",
	  "oauth": {
	    "authServerMetadataUrl": "https://auth.example.com/.well-known/oauth-authorization-server",
	    "callbackPort": 19485,
	    "clientId": "client-1",
	    "xaa": true
	  }
	},
        "http_fixture": {
          "type": "http",
          "url": "http://127.0.0.1:8124/mcp",
          "headers": {"X-Test": "present"},
          "disabled": true
        },
        "inferred_http": {
          "url": "http://127.0.0.1:8125/mcp"
        },
        "unsupported_ws": {
          "type": "ws",
          "url": "ws://127.0.0.1:8126/mcp"
        },
        "invalid_stdio": {
          "type": "stdio",
          "args": ["missing-command"]
        }
      }
    })JSON", loom::services::mcp::ConfigScope::Project);

    ASSERT_TRUE(parsed.has_value()) << static_cast<int>(parsed.error());
    ASSERT_EQ(parsed->size(), 4u);

    const auto& stdio = parsed->at("stdio_fixture");
    EXPECT_EQ(stdio.transport, loom::services::mcp::TransportType::Stdio);
    EXPECT_EQ(stdio.command, "node");
    ASSERT_EQ(stdio.args.size(), 2u);
    EXPECT_EQ(stdio.args[0], "server.js");
    EXPECT_EQ(stdio.args[1], "--flag");
    EXPECT_EQ(stdio.env.at("FOO"), "bar");
    EXPECT_EQ(stdio.timeout, std::chrono::milliseconds{1234});
    EXPECT_FALSE(stdio.auto_start);
    EXPECT_FALSE(stdio.enabled);
    EXPECT_EQ(stdio.scope, loom::services::mcp::ConfigScope::Project);

	const auto& sse = parsed->at("sse_fixture");
	EXPECT_EQ(sse.transport, loom::services::mcp::TransportType::Sse);
	EXPECT_EQ(sse.url, "http://127.0.0.1:8123/events");
	EXPECT_EQ(sse.headers.at("Authorization"), "Bearer token");
	EXPECT_EQ(sse.headers_helper, "node helper.js");
	ASSERT_TRUE(sse.oauth.has_value());
	ASSERT_TRUE(sse.oauth->auth_server_metadata_url.has_value());
	EXPECT_EQ(*sse.oauth->auth_server_metadata_url, "https://auth.example.com/.well-known/oauth-authorization-server");
	ASSERT_TRUE(sse.oauth->callback_port.has_value());
	EXPECT_EQ(*sse.oauth->callback_port, 19485);
	ASSERT_TRUE(sse.oauth->client_id.has_value());
	EXPECT_EQ(*sse.oauth->client_id, "client-1");
	EXPECT_TRUE(sse.oauth->xaa);

    const auto& http = parsed->at("http_fixture");
    EXPECT_EQ(http.transport, loom::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(http.url, "http://127.0.0.1:8124/mcp");
    EXPECT_EQ(http.headers.at("X-Test"), "present");
    EXPECT_FALSE(http.enabled);

    const auto& inferred = parsed->at("inferred_http");
    EXPECT_EQ(inferred.transport, loom::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(inferred.url, "http://127.0.0.1:8125/mcp");
    EXPECT_FALSE(parsed->contains("unsupported_ws"));
    EXPECT_FALSE(parsed->contains("invalid_stdio"));
}

TEST(McpConfigParser, SupportsServersAliasAndRejectsInvalidJson) {
    const auto parsed = loom::services::mcp::ConfigParser::parse_json(R"JSON({
      "servers": {
        "alias_fixture": {
          "transport": "streamable-http",
          "url": "http://127.0.0.1:8127/mcp"
        }
      }
    })JSON", loom::services::mcp::ConfigScope::User);

    ASSERT_TRUE(parsed.has_value()) << static_cast<int>(parsed.error());
    ASSERT_EQ(parsed->size(), 1u);
    const auto& alias = parsed->at("alias_fixture");
    EXPECT_EQ(alias.transport, loom::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(alias.scope, loom::services::mcp::ConfigScope::User);

    const auto invalid = loom::services::mcp::ConfigParser::parse_json(
        "{",
        loom::services::mcp::ConfigScope::User
    );
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), loom::services::mcp::ConfigError::ParseError);
}

TEST(McpTypes, JsonRpcSerializationIncludesParams) {
    auto request = loom::services::mcp::make_request(
        int64_t{7},
        "tools/call",
        std::optional<std::string>{R"({"name":"echo","arguments":{"value":"hello"}})"});

    const auto serialized = loom::services::mcp::serialize_request(request);

    EXPECT_NE(serialized.find(R"("method":"tools/call")"), std::string::npos);
    EXPECT_NE(serialized.find(R"("params":{"name":"echo","arguments":{"value":"hello"}})"), std::string::npos);

    auto notification = loom::services::mcp::make_notification(
        "notifications/initialized",
        std::optional<std::string>{R"({"ready":true})"});

    const auto serialized_notification = loom::services::mcp::serialize_notification(notification);
    EXPECT_NE(serialized_notification.find(R"("params":{"ready":true})"), std::string::npos);
}


// RFC-0001 B4: persisted-data round-trip coverage for the canonical
// loom.config.mcp_types settings shape — legacy snake_case reads, project
// layering, and environment-layer non-interference.

TEST(McpTypes, ReadsOldShapedSnakeCaseAndRewritesCamelCase) {
    const auto root = c13_make_temp_root("loom_mcp_types_legacy_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        // Every legacy snake_case key the parser historically accepted, plus
        // "transport" (instead of "type") and the previously dropped keys
        // "disabled" / oauth "issuer"; configScope is intentionally absent
        // so the "project" default path is exercised.
        file << R"JSON({
  "mcpServers": {
    "legacy": {
      "transport": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Test": "present"},
      "headers_helper": "node headers.js",
      "disabled": false,
      "oauth": {
        "auth_server_metadata_url": "https://auth.example.com/.well-known/oauth-authorization-server",
        "callback_port": 19485,
        "client_id": "client-1",
        "xaa": true,
        "issuer": "https://issuer.example.com"
      }
    }
  }
})JSON";
    }

    auto assert_legacy_fields = [](const loom::core::ConfigManager& manager) {
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        const auto& server = manager.settings().mcp_servers.front();
        EXPECT_EQ(server.name, "legacy");
        EXPECT_EQ(server.transport, "http");
        ASSERT_TRUE(server.url.has_value());
        EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
        EXPECT_EQ(server.headers.at("X-Test"), "present");
        ASSERT_TRUE(server.headers_helper.has_value());
        EXPECT_EQ(*server.headers_helper, "node headers.js");
        ASSERT_TRUE(server.oauth.has_value());
        ASSERT_TRUE(server.oauth->auth_server_metadata_url.has_value());
        EXPECT_EQ(*server.oauth->auth_server_metadata_url,
                  "https://auth.example.com/.well-known/oauth-authorization-server");
        ASSERT_TRUE(server.oauth->callback_port.has_value());
        EXPECT_EQ(*server.oauth->callback_port, 19485);
        ASSERT_TRUE(server.oauth->client_id.has_value());
        EXPECT_EQ(*server.oauth->client_id, "client-1");
        EXPECT_TRUE(server.oauth->xaa);
        // Round-trip guarantee (RFC-0001 B followup c4): the previously
        // dropped fields now survive. "disabled" is optional<bool>, so an
        // explicit false is captured rather than defaulted away; oauth
        // "issuer" is read in canonical spelling; and an absent configScope
        // falls back to the "project" default.
        ASSERT_TRUE(server.disabled.has_value());
        EXPECT_FALSE(*server.disabled);
        ASSERT_TRUE(server.oauth->issuer.has_value());
        EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");
        EXPECT_EQ(server.config_scope, "project");
    };

    {
        loom::core::ConfigManager loaded(project_path);
        ASSERT_TRUE(loaded.load().has_value());
        assert_legacy_fields(loaded);

        // Re-save: the serializer must rewrite every READ field in canonical
        // camelCase with zero loss.
        ASSERT_TRUE(loaded.save().has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"headersHelper\""), std::string::npos);
    EXPECT_EQ(rewritten.find("headers_helper"), std::string::npos);
    EXPECT_NE(rewritten.find("\"authServerMetadataUrl\""), std::string::npos);
    EXPECT_EQ(rewritten.find("auth_server_metadata_url"), std::string::npos);
    EXPECT_NE(rewritten.find("\"callbackPort\""), std::string::npos);
    EXPECT_EQ(rewritten.find("callback_port"), std::string::npos);
    EXPECT_NE(rewritten.find("\"clientId\""), std::string::npos);
    EXPECT_EQ(rewritten.find("client_id"), std::string::npos);
    EXPECT_NE(rewritten.find("\"type\": \"http\""), std::string::npos);
    EXPECT_EQ(rewritten.find("\"transport\""), std::string::npos);
    // Previously dropped fields are rewritten in canonical form; configScope
    // is always written (defaulting to "project"), never in snake_case.
    EXPECT_NE(rewritten.find("\"disabled\": false"), std::string::npos);
    EXPECT_NE(rewritten.find("\"issuer\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"project\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    {
        loom::core::ConfigManager reloaded(project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        assert_legacy_fields(reloaded);
    }

    fs::remove_all(root);
}


// RFC-0001 B followup c4: canonical camelCase input with an explicitly
// disabled server and an oauth issuer must survive load -> save -> reload
// byte-for-byte in meaning, and be rewritten in canonical spelling.

TEST(McpTypes, DisabledAndOauthIssuerSurviveConfigRewrite) {
    const auto root = c13_make_temp_root("loom_mcp_types_disabled_issuer_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "canonical": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "disabled": true,
      "configScope": "user",
      "oauth": {
        "issuer": "https://issuer.example.com"
      }
    }
  }
})JSON";
    }

    {
        loom::core::ConfigManager loaded(project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        const auto& server = loaded.settings().mcp_servers.front();
        ASSERT_TRUE(server.disabled.has_value());
        EXPECT_TRUE(*server.disabled);
        EXPECT_EQ(server.config_scope, "user");
        ASSERT_TRUE(server.oauth.has_value());
        ASSERT_TRUE(server.oauth->issuer.has_value());
        EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");

        ASSERT_TRUE(loaded.save().has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"disabled\": true"), std::string::npos);
    EXPECT_NE(rewritten.find("\"issuer\": \"https://issuer.example.com\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"user\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    loom::core::ConfigManager reloaded(project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& server = reloaded.settings().mcp_servers.front();
    ASSERT_TRUE(server.disabled.has_value());
    EXPECT_TRUE(*server.disabled);
    EXPECT_EQ(server.config_scope, "user");
    ASSERT_TRUE(server.oauth.has_value());
    ASSERT_TRUE(server.oauth->issuer.has_value());
    EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");

    fs::remove_all(root);
}


// RFC-0001 B followup c4: configScope reads canonical camelCase and legacy
// snake_case alike, but every rewrite emits only the canonical camelCase
// key, including across a reload.

TEST(McpTypes, ConfigScopeRoundTripsInCanonicalCamelCase) {
    const auto root = c13_make_temp_root("loom_mcp_types_scope_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "legacy-scope": {
      "command": "node",
      "args": ["legacy.js"],
      "config_scope": "user"
    },
    "canonical-scope": {
      "command": "node",
      "args": ["canonical.js"],
      "configScope": "local"
    },
    "both-scopes": {
      "command": "node",
      "args": ["both.js"],
      "configScope": "local",
      "config_scope": "user"
    }
  }
})JSON";
    }

    {
        loom::core::ConfigManager loaded(project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 3u);
        EXPECT_EQ(loaded.settings().mcp_servers[0].config_scope, "user");
        EXPECT_EQ(loaded.settings().mcp_servers[1].config_scope, "local");
        // When both spellings are present, canonical camelCase wins
        // (json_string(...).or_else(...) short-circuits on the first hit).
        EXPECT_EQ(loaded.settings().mcp_servers[2].config_scope, "local");

        ASSERT_TRUE(loaded.save().has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"configScope\": \"user\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"local\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    loom::core::ConfigManager reloaded(project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 3u);
    EXPECT_EQ(reloaded.settings().mcp_servers[0].config_scope, "user");
    EXPECT_EQ(reloaded.settings().mcp_servers[1].config_scope, "local");
    EXPECT_EQ(reloaded.settings().mcp_servers[2].config_scope, "local");

    fs::remove_all(root);
}


// RFC-0001 B followup c4: absent optional keys stay unset. A minimal server
// must round-trip without a "disabled" or oauth "issuer" key in the rewrite,
// while configScope is still written with its "project" default.

TEST(McpTypes, AbsentDisabledAndIssuerKeysStayUnset) {
    const auto root = c13_make_temp_root("loom_mcp_types_absent_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "minimal": {
      "command": "node"
    }
  }
})JSON";
    }

    {
        loom::core::ConfigManager loaded(project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        const auto& server = loaded.settings().mcp_servers.front();
        EXPECT_FALSE(server.disabled.has_value());
        EXPECT_FALSE(server.oauth.has_value());
        EXPECT_EQ(server.config_scope, "project");

        ASSERT_TRUE(loaded.save().has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_EQ(rewritten.find("disabled"), std::string::npos);
    EXPECT_EQ(rewritten.find("issuer"), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"project\""), std::string::npos);

    loom::core::ConfigManager reloaded(project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& server = reloaded.settings().mcp_servers.front();
    EXPECT_FALSE(server.disabled.has_value());
    EXPECT_FALSE(server.oauth.has_value());
    EXPECT_EQ(server.config_scope, "project");

    fs::remove_all(root);
}


// RFC-0001 B followup c12: the xaaIdp section (issuer/clientId/callbackPort)
// used to be dropped by the hand-rolled full-save serializer. All three
// values must survive an unrelated mutation + rewrite + reload, and the
// rewritten document must carry the canonical camelCase keys exactly once.
// Bad-typed members are tolerated (folded-in second phase below).

TEST(McpTypes, XaaIdpRoundTripsConfigRewrite) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = c13_make_temp_root("loom_mcp_types_xaa_idp_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "model": "test-model",
  "mcpServers": {
    "keep-me": {
      "command": "node",
      "args": ["serve.js"]
    }
  },
  "xaaIdp": {
    "issuer": "https://idp.example.com",
    "clientId": "loom-cli",
    "callbackPort": 8765
  }
})JSON";
    }

    {
        loom::core::ConfigManager loaded(project_path);
        ASSERT_TRUE(loaded.load().has_value());
        const auto& xaa = loaded.settings().xaa_idp;
        EXPECT_EQ(xaa.issuer, "https://idp.example.com");
        EXPECT_EQ(xaa.client_id, "loom-cli");
        ASSERT_TRUE(xaa.callback_port.has_value());
        EXPECT_EQ(*xaa.callback_port, 8765);
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(loaded.settings().mcp_servers.front().name, "keep-me");
        EXPECT_EQ(loaded.settings().model.default_model, "test-model");

        // Mutate something unrelated (display theme), then full-save.
        loaded.settings_mut().display.theme = "dark";
        ASSERT_TRUE(loaded.save().has_value());
    }

    // Byte-semantic check of the rewritten document.
    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    auto doc = loom::utils::json::parse(rewritten);
    ASSERT_TRUE(doc.has_value());
    const auto xaa_obj = doc->root().get("xaaIdp");
    ASSERT_TRUE(xaa_obj.is_obj());
    EXPECT_EQ(std::string(xaa_obj.get("issuer").as_str()), "https://idp.example.com");
    EXPECT_EQ(std::string(xaa_obj.get("clientId").as_str()), "loom-cli");
    ASSERT_TRUE(xaa_obj.get("callbackPort").is_num());
    EXPECT_EQ(xaa_obj.get("callbackPort").as_int(), 8765);
    // Canonical camelCase only; the section appears exactly once.
    EXPECT_EQ(rewritten.find("client_id"), std::string::npos);
    EXPECT_EQ(rewritten.find("callback_port"), std::string::npos);
    ASSERT_NE(rewritten.find("xaaIdp"), std::string::npos);
    EXPECT_EQ(rewritten.find("xaaIdp"), rewritten.rfind("xaaIdp"));
    // The unrelated mcp server and the mutation survive untouched.
    const auto servers = doc->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto keep = servers.get("keep-me");
    ASSERT_TRUE(keep.is_obj());
    EXPECT_EQ(std::string(keep.get("command").as_str()), "node");
    EXPECT_EQ(std::string(doc->root().get("theme").as_str()), "dark");

    // Reload: all three XAA values survive.
    loom::core::ConfigManager reloaded(project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    const auto& xaa = reloaded.settings().xaa_idp;
    EXPECT_EQ(xaa.issuer, "https://idp.example.com");
    EXPECT_EQ(xaa.client_id, "loom-cli");
    ASSERT_TRUE(xaa.callback_port.has_value());
    EXPECT_EQ(*xaa.callback_port, 8765);
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    EXPECT_EQ(reloaded.settings().mcp_servers.front().name, "keep-me");

    fs::remove_all(root);

    // Bad-type tolerance: a numeric issuer and a string callbackPort are
    // ignored without failing the load, while a well-typed clientId in the
    // same object still parses.
    const auto bad_root = fs::temp_directory_path() /
        ("loom_mcp_types_xaa_idp_bad_" + std::to_string(suffix));
    fs::create_directories(bad_root);
    const auto bad_project = bad_root / "project.json";
    {
        std::ofstream file(bad_project);
        file << R"JSON({
  "xaaIdp": {
    "issuer": 42,
    "clientId": "partial-client",
    "callbackPort": "8080"
  }
})JSON";
    }
    loom::core::ConfigManager bad(bad_project);
    ASSERT_TRUE(bad.load().has_value());
    const auto& bad_xaa = bad.settings().xaa_idp;
    EXPECT_TRUE(bad_xaa.issuer.empty());
    EXPECT_EQ(bad_xaa.client_id, "partial-client");
    EXPECT_FALSE(bad_xaa.callback_port.has_value());

    fs::remove_all(bad_root);
}


// RFC-0001 B followup c12: default settings never emit an xaaIdp section,
// and a save/load/save cycle with the section absent is byte-stable (this
// is also what makes /mcp xaa clear remove the section on rewrite).

TEST(McpTypes, XaaIdpOmittedWhenUnset) {
    const auto root = c13_make_temp_root("loom_mcp_types_xaa_idp_absent_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto project_path = root / "project.json";

    std::string first;
    {
        loom::core::ConfigManager fresh(project_path);
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_TRUE(fresh.settings().xaa_idp.issuer.empty());
        EXPECT_TRUE(fresh.settings().xaa_idp.client_id.empty());
        EXPECT_FALSE(fresh.settings().xaa_idp.callback_port.has_value());
        ASSERT_TRUE(fresh.save().has_value());

        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        first = buffer.str();
    }
    EXPECT_EQ(first.find("xaaIdp"), std::string::npos);

    {
        loom::core::ConfigManager reloaded(project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_TRUE(reloaded.settings().xaa_idp.issuer.empty());
        EXPECT_FALSE(reloaded.settings().xaa_idp.callback_port.has_value());
        ASSERT_TRUE(reloaded.save().has_value());
    }
    std::string second;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        second = buffer.str();
    }
    EXPECT_EQ(second.find("xaaIdp"), std::string::npos);
    EXPECT_EQ(first, second);

    fs::remove_all(root);
}

TEST(McpTypes, EnvironmentLayerLeavesMcpServersUntouched) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_types_env_test_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto project_path = root / "project.json";

    EnvironmentGuard api_key_guard("LOOM_API_KEY", "env-layer-test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", "https://env.example.com");

    {
        std::ofstream project_file(project_path);
        project_file << R"JSON({
  "mcpServers": {
    "remote": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Test": "present"},
      "headersHelper": "node headers.js"
    }
  }
})JSON";
    }

    loom::core::ConfigManager manager(project_path);
    ASSERT_TRUE(manager.load().has_value());

    // The environment layer demonstrably ran...
    ASSERT_TRUE(manager.settings().network.api_key.has_value());
    EXPECT_EQ(*manager.settings().network.api_key, "env-layer-test-key");
    ASSERT_TRUE(manager.settings().network.base_url.has_value());
    EXPECT_EQ(*manager.settings().network.base_url, "https://env.example.com");

    // ...and left the file-defined MCP servers byte-for-byte intact.
    ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
    const auto& server = manager.settings().mcp_servers.front();
    EXPECT_EQ(server.name, "remote");
    EXPECT_EQ(server.transport, "http");
    ASSERT_TRUE(server.url.has_value());
    EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
    EXPECT_EQ(server.headers.at("X-Test"), "present");
    ASSERT_TRUE(server.headers_helper.has_value());
    EXPECT_EQ(*server.headers_helper, "node headers.js");

    fs::remove_all(root);
}


// ============================================================================
// RFC-0001 B followup c6: per-tier MCP file routing (design doc groups 2-13)
// ============================================================================



// Group 2: three physical files, lowest-to-highest precedence; a name in all
// three resolves to the local value.

TEST(McpTypes, McpStorageThreeFilePrecedence) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_three_file_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(user_path, R"JSON({
  "mcpServers": {
    "onlyu": {"command": "node", "args": ["u.js"]},
    "shared": {"command": "node", "args": ["user.js"]}
  }
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "onlyp": {"command": "node", "args": ["p.js"]},
    "shared": {"command": "node", "args": ["project.js"]}
  }
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {
    "onlyl": {"command": "node", "args": ["l.js"]},
    "shared": {"command": "node", "args": ["local.js"]}
  }
})JSON");

    loom::core::ConfigManager manager(user_path, project_path, local_path);
    const auto paths = manager.mcp_scope_paths();
    ASSERT_EQ(paths.size(), 3u);
    EXPECT_EQ(paths[0].first, loom::core::McpStorageScope::User);
    EXPECT_EQ(paths[1].first, loom::core::McpStorageScope::Project);
    EXPECT_EQ(paths[2].first, loom::core::McpStorageScope::Local);

    ASSERT_TRUE(manager.load().has_value());
    std::map<std::string, loom::core::McpServerConfig> by_name;
    for (const auto& server : manager.settings().mcp_servers) {
        by_name[server.name] = server;
    }
    ASSERT_EQ(by_name.size(), 4u);
    EXPECT_EQ(by_name.at("shared").args, (std::vector<std::string>{"local.js"}));
    EXPECT_EQ(by_name.at("onlyu").args, (std::vector<std::string>{"u.js"}));
    EXPECT_EQ(by_name.at("onlyp").args, (std::vector<std::string>{"p.js"}));
    EXPECT_EQ(by_name.at("onlyl").args, (std::vector<std::string>{"l.js"}));

    EXPECT_EQ(*manager.mcp_server_owner("shared"), loom::core::McpStorageScope::Local);
    EXPECT_EQ(*manager.mcp_server_owner("onlyu"),  loom::core::McpStorageScope::User);
    EXPECT_EQ(*manager.mcp_server_owner("onlyp"),  loom::core::McpStorageScope::Project);
    EXPECT_EQ(*manager.mcp_server_owner("onlyl"),  loom::core::McpStorageScope::Local);

    const auto shared_files = manager.find_mcp_server_files("shared");
    ASSERT_EQ(shared_files.size(), 3u);
    EXPECT_EQ(shared_files.back().first, loom::core::McpStorageScope::Local);
    EXPECT_TRUE(manager.find_mcp_server_files("onlyu").size() == 1u);

    fs::remove_all(root);
}


// Groups 3 + 8: upsert(User) writes ONLY the user file (no model/display/
// features sections, other files untouched), and the patched entry is
// compared STRUCTURALLY with the C4-pinned key set/order preserved.

TEST(McpTypes, McpUpsertUserWritesOnlyUserFile) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_user_upsert_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    loom::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "http";
    cfg.args = {"a1", "a2"};
    cfg.env = {{"K", "V"}};
    cfg.url = "https://mcp.example.com/mcp";
    cfg.headers = {{"Authorization", "Bearer abc"}};
    cfg.headers_helper = "node h.js";
    cfg.disabled = false;
    cfg.config_scope = "user";
    loom::core::McpOAuthConfig oauth;
    oauth.auth_server_metadata_url = "https://auth.example.com/meta";
    oauth.callback_port = 19485;
    oauth.client_id = "client-1";
    oauth.xaa = true;
    oauth.issuer = "https://issuer.example.com";
    cfg.oauth = oauth;

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto upserted = manager.upsert_mcp_server(loom::core::McpStorageScope::User, cfg);
        ASSERT_TRUE(upserted.has_value()) << upserted.error().message;
    }

    EXPECT_TRUE(fs::exists(user_path));
    EXPECT_FALSE(fs::exists(project_path));
    EXPECT_FALSE(fs::exists(local_path));

    auto parsed = loom::utils::json::parse_file(user_path);
    ASSERT_TRUE(parsed.has_value());
    // The file contains exactly one top-level section: mcpServers.
    const auto root_keys = c6_json_keys(parsed->root());
    ASSERT_EQ(root_keys.size(), 1u);
    EXPECT_EQ(root_keys[0], "mcpServers");

    const auto servers = parsed->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto entry = servers.get("srv");
    ASSERT_TRUE(entry.is_obj());

    // Key order is exactly the C4-pinned set (command omitted as empty).
    const auto keys = c6_json_keys(entry);
    EXPECT_EQ(keys, (std::vector<std::string>{
        "type", "args", "env", "url", "headers", "headersHelper",
        "disabled", "configScope", "oauth"}));
    EXPECT_EQ(entry.get("type").as_str(), std::string_view("http"));
    ASSERT_TRUE(entry.get("args").is_arr());
    EXPECT_EQ(entry.get("args").at(0).as_str(), std::string_view("a1"));
    EXPECT_EQ(entry.get("args").at(1).as_str(), std::string_view("a2"));
    EXPECT_EQ(entry.get("env").get("K").as_str(), std::string_view("V"));
    EXPECT_EQ(entry.get("url").as_str(), std::string_view("https://mcp.example.com/mcp"));
    EXPECT_EQ(entry.get("headers").get("Authorization").as_str(),
              std::string_view("Bearer abc"));
    EXPECT_EQ(entry.get("headersHelper").as_str(), std::string_view("node h.js"));
    EXPECT_FALSE(entry.get("disabled").as_bool());
    EXPECT_EQ(entry.get("configScope").as_str(), std::string_view("user"));
    const auto oauth_json = entry.get("oauth");
    ASSERT_TRUE(oauth_json.is_obj());
    EXPECT_EQ(c6_json_keys(oauth_json), (std::vector<std::string>{
        "authServerMetadataUrl", "callbackPort", "clientId", "xaa", "issuer"}));
    EXPECT_EQ(oauth_json.get("callbackPort").as_int(), 19485);
    EXPECT_EQ(oauth_json.get("clientId").as_str(), std::string_view("client-1"));
    EXPECT_TRUE(oauth_json.get("xaa").as_bool());

    // Reload picks the user entry up.
    {
        loom::core::ConfigManager reloaded(user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "srv");
        EXPECT_EQ(*reloaded.settings().mcp_servers[0].url, "https://mcp.example.com/mcp");

        // Same-name upsert replaces, never duplicates.
        cfg.url = "https://mcp.example.com/v2";
        ASSERT_TRUE(reloaded.upsert_mcp_server(loom::core::McpStorageScope::User, cfg)
                        .has_value());
    }
    auto reparsed = loom::utils::json::parse_file(user_path);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->root().get("mcpServers").size(), 1u);
    EXPECT_EQ(reparsed->root().get("mcpServers").get("srv").get("url").as_str(),
              std::string_view("https://mcp.example.com/v2"));

    fs::remove_all(root);
}


// Group 8 (stdio variant): the patched file is whole-file yyjson-PRETTY
// (inline arrays become multiline), the key order survives, and a reload
// round-trips every field structurally.

TEST(McpTypes, McpPatchedEntryStructuralShapeAndKeyOrder) {
    const auto root = c13_make_temp_root("loom_mcp_c6_shape_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    loom::core::McpServerConfig cfg;
    cfg.name = "runner";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.args = {"serve.js", "--port", "9000"};
    cfg.env = {{"DEBUG", "1"}};
    cfg.disabled = true;
    cfg.config_scope = "project";

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Project, cfg)
                        .has_value());
    }

    const std::string text = c6_read_file(project_path);
    // Pretty reformat: the inline args array is now multiline.
    EXPECT_NE(text.find("\"args\": [\n"), std::string::npos);
    auto parsed = loom::utils::json::parse_file(project_path);
    ASSERT_TRUE(parsed.has_value());
    const auto entry = parsed->root().get("mcpServers").get("runner");
    ASSERT_TRUE(entry.is_obj());
    EXPECT_EQ(c6_json_keys(entry),
              (std::vector<std::string>{"type", "command", "args", "env",
                                        "disabled", "configScope"}));
    EXPECT_EQ(entry.get("type").as_str(), std::string_view("stdio"));
    EXPECT_EQ(entry.get("command").as_str(), std::string_view("node"));
    EXPECT_EQ(entry.get("args").size(), 3u);
    EXPECT_TRUE(entry.get("disabled").as_bool());

    loom::core::ConfigManager reloaded(user_path, project_path, local_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& back = reloaded.settings().mcp_servers[0];
    EXPECT_EQ(back.command, "node");
    EXPECT_EQ(back.args, (std::vector<std::string>{"serve.js", "--port", "9000"}));
    EXPECT_EQ(back.env.at("DEBUG"), "1");
    ASSERT_TRUE(back.disabled.has_value());
    EXPECT_TRUE(*back.disabled);
    EXPECT_EQ(back.config_scope, "project");

    fs::remove_all(root);
}


// Group 4: the default Local scope creates settings.local.json (no
// settings.json) and appends its basename to ./.gitignore exactly once,
// coping with a pre-existing file that has no trailing newline.

TEST(McpTypes, McpUpsertLocalCreatesLocalFileAndGitignore) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_local_gitignore_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    // c13e: .gitignore appends happen only inside a git work tree.
    ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);

    // Existing .gitignore with NO trailing newline.
    c6_write_file(root / ".gitignore", "sentinel");

    const auto user_path    = root / "user.json";
    const auto project_path = root / ".loom" / "settings.json";
    const auto local_path   = root / ".loom" / "settings.local.json";

    auto make_stdio = [](std::string name) {
        loom::core::McpServerConfig cfg;
        cfg.name = std::move(name);
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.args = {"s.js"};
        cfg.config_scope = "local";
        return cfg;
    };

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Local,
                                              make_stdio("l1")).has_value());
    }
    EXPECT_TRUE(fs::exists(local_path));
    EXPECT_FALSE(fs::exists(project_path));
    // c13d: the local lock sibling is ignored alongside the data file.
    EXPECT_EQ(c6_read_file(root / ".gitignore"),
              "sentinel\nsettings.local.json\nsettings.local.json.lock\n");

    // The local tier is the documented secret holder: owner-only (0600).
    {
        std::error_code ec;
        const auto status = fs::status(local_path, ec);
        ASSERT_FALSE(ec);
        const auto perms = status.permissions();
        const auto none = fs::perms::none;
        EXPECT_EQ(perms & (fs::perms::group_read | fs::perms::group_write |
                           fs::perms::group_exec | fs::perms::others_read |
                           fs::perms::others_write | fs::perms::others_exec),
                  none);
        EXPECT_NE(perms & fs::perms::owner_read, none);
        EXPECT_NE(perms & fs::perms::owner_write, none);
    }

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Local,
                                              make_stdio("l2")).has_value());
    }
    const std::string gitignore = c6_read_file(root / ".gitignore");
    // Line-exact idempotency (the lock line contains the data basename as
    // a substring, so count whole lines).
    EXPECT_EQ(c6_count_occurrences(gitignore, "settings.local.json\n"), 1u);
    EXPECT_EQ(c6_count_occurrences(gitignore, "settings.local.json.lock\n"), 1u);
    auto parsed = loom::utils::json::parse_file(local_path);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->root().get("mcpServers").size(), 2u);

    fs::remove_all(root);
}


// Group 6: default remove clears every physical copy and is idempotent;
// --scope touches one file; unknown names yield an empty outcome; removing
// the last entry drops mcpServers but preserves sibling sections.

TEST(McpTypes, McpRemoveAllCopiesScopedAndNotFound) {
    const auto root = c13_make_temp_root("loom_mcp_c6_remove_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // Default removal touches both polluted copies.
    c6_write_file(user_path, R"JSON({
  "mcpServers": {
    "dup": {"command": "node", "args": ["u.js"]},
    "uonly": {"command": "node", "args": ["uo.js"]}
  }
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "dup": {"command": "node", "args": ["p.js"]},
    "ponly": {"command": "node", "args": ["po.js"]}
  }
})JSON");
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 2u);
        EXPECT_TRUE(outcome->failed.empty());

        // Idempotent re-run: nothing present anymore.
        auto again = manager.remove_mcp_server("dup");
        ASSERT_TRUE(again.has_value());
        EXPECT_TRUE(again->touched.empty());
        EXPECT_TRUE(again->failed.empty());
    }
    {
        loom::core::ConfigManager reloaded(user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        std::set<std::string> names;
        for (const auto& server : reloaded.settings().mcp_servers) names.insert(server.name);
        EXPECT_EQ(names, (std::set<std::string>{"uonly", "ponly"}));
    }

    // Scoped removal touches only that tier.
    c6_write_file(user_path, R"JSON({"mcpServers": {"s": {"command": "node"}}})JSON");
    c6_write_file(project_path, R"JSON({"mcpServers": {"s": {"command": "node"}}})JSON");
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server(
            "s", loom::core::McpStorageScope::Project);
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 1u);
        EXPECT_EQ(outcome->touched[0], project_path);
    }
    {
        loom::core::ConfigManager reloaded(user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "s");  // survives in user
    }

    // Unknown names: empty outcome both unscoped and scoped.
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        EXPECT_TRUE(manager.remove_mcp_server("ghost")->touched.empty());
        auto scoped = manager.remove_mcp_server("ghost", loom::core::McpStorageScope::Local);
        ASSERT_TRUE(scoped.has_value());
        EXPECT_TRUE(scoped->touched.empty());
    }

    // Removing the last entry drops mcpServers but keeps sibling sections.
    c6_write_file(project_path, R"JSON({
  "systemPrompt": "keep",
  "mcpServers": {"last": {"command": "node"}}
})JSON");
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.remove_mcp_server(
            "last", loom::core::McpStorageScope::Project).has_value());
        auto parsed = loom::utils::json::parse_file(project_path);
        ASSERT_TRUE(parsed.has_value());
        EXPECT_FALSE(parsed->root().has("mcpServers"));
        EXPECT_EQ(parsed->root().get("systemPrompt").as_str(), std::string_view("keep"));
    }

    fs::remove_all(root);
}


// Group 7: enable/disable patch only "disabled" on the owner file; unknown
// sibling keys survive; lower tiers are untouched; the batch helper patches
// one file per distinct owner.

TEST(McpTypes, McpEnableDisablePatchesOwnerFiles) {
    const auto root = c13_make_temp_root("loom_mcp_c6_disable_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "pon": {"command": "node"},
    "px": {"command": "node", "weird": 123}
  }
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {"lon": {"command": "node"}}
})JSON");

    const std::string project_before = c6_read_file(project_path);
    const std::string local_before = c6_read_file(local_path);

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());

        // Unknown sibling key survives a disable/enable round trip.
        ASSERT_TRUE(manager.set_mcp_server_disabled("px", true).has_value());
        ASSERT_TRUE(manager.set_mcp_server_disabled("px", false).has_value());
        auto project_doc = loom::utils::json::parse_file(project_path);
        ASSERT_TRUE(project_doc.has_value());
        const auto px = project_doc->root().get("mcpServers").get("px");
        EXPECT_EQ(px.get("weird").as_int(), 123);
        EXPECT_FALSE(px.get("disabled").as_bool());

        // Project owner patches project; local bytes untouched.
        ASSERT_TRUE(manager.set_mcp_server_disabled("pon", true).has_value());
        EXPECT_EQ(c6_read_file(local_path), local_before);

        // Unknown name and wrong-scope name are errors.
        EXPECT_FALSE(manager.set_mcp_server_disabled("zzz", true).has_value());
        EXPECT_FALSE(manager.set_mcp_server_disabled(
            "pon", true, loom::core::McpStorageScope::Local).has_value());
    }
    EXPECT_NE(c6_read_file(project_path), project_before);

    // "all" shape: one batch call per distinct owner file.
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::vector<std::string> project_names{"pon", "px"};
        const std::vector<std::string> local_names{"lon"};
        EXPECT_TRUE(manager.set_mcp_servers_disabled_in(
            loom::core::McpStorageScope::Project, project_names, true).has_value());
        EXPECT_TRUE(manager.set_mcp_servers_disabled_in(
            loom::core::McpStorageScope::Local, local_names, true).has_value());
        for (const auto& [path, names] : {
                 std::pair{project_path, std::vector<std::string>{"pon", "px"}},
                 std::pair{local_path, std::vector<std::string>{"lon"}}}) {
            auto doc = loom::utils::json::parse_file(path);
            ASSERT_TRUE(doc.has_value()) << path.string();
            for (const auto& name : names) {
                EXPECT_TRUE(doc->root().get("mcpServers").get(name)
                                .get("disabled").as_bool()) << path.string() << " " << name;
            }
        }
    }

    fs::remove_all(root);
}


// Group 9 (§A): key=value user content and a non-object local root make
// those tiers contribute zero entries with one diagnostic each, while the
// project tier keeps loading; upserts against an unparseable tier
// fail with an actionable message and leave bytes untouched.

TEST(McpTypes, McpGarbageUserLocalFilesSkippedAndUpsertRejected) {
    const auto root = c13_make_temp_root("loom_mcp_c6_garbage_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(user_path, "FOO=bar\nBAZ=qux\n");              // not JSON
    c6_write_file(local_path, "[1, 2]\n");                        // valid JSON, wrong root
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "g1": {"command": "node"},
    "p1": {"command": "node"}
  }
})JSON");

    const std::string user_before = c6_read_file(user_path);
    const std::string local_before = c6_read_file(local_path);

    loom::core::ConfigManager manager(user_path, project_path, local_path);
    ASSERT_TRUE(manager.load().has_value());
    std::set<std::string> names;
    for (const auto& server : manager.settings().mcp_servers) names.insert(server.name);
    EXPECT_EQ(names, (std::set<std::string>{"g1", "p1"}));

    loom::core::McpServerConfig cfg;
    cfg.name = "newu";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.config_scope = "user";
    auto user_upsert = manager.upsert_mcp_server(loom::core::McpStorageScope::User, cfg);
    ASSERT_FALSE(user_upsert.has_value());
    EXPECT_NE(user_upsert.error().message.find(user_path.string()), std::string::npos);
    EXPECT_NE(user_upsert.error().message.find("not valid JSON"), std::string::npos);
    EXPECT_EQ(c6_read_file(user_path), user_before);

    cfg.name = "newl";
    auto local_upsert = manager.upsert_mcp_server(loom::core::McpStorageScope::Local, cfg);
    ASSERT_FALSE(local_upsert.has_value());
    EXPECT_EQ(c6_read_file(local_path), local_before);

    // Other tiers remain patchable.
    cfg.name = "newp";
    ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Project, cfg)
                    .has_value());
    loom::core::ConfigManager reloaded(user_path, project_path, local_path);
    ASSERT_TRUE(reloaded.load().has_value());
    names.clear();
    for (const auto& server : reloaded.settings().mcp_servers) names.insert(server.name);
    EXPECT_EQ(names, (std::set<std::string>{"g1", "newp", "p1"}));

    fs::remove_all(root);
}


// Group 10: $LOOM_CONFIG_DIR routes the user tier; config_home_write() and
// the default ConfigManager constructor both honor it.

TEST(McpTypes, McpUserScopeHonorsConfigDirEnv) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_env_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto fake_home = root / "home";
    const auto config_dir = root / "cfgdir";
    const auto work = root / "work";
    fs::create_directories(fake_home);
    fs::create_directories(config_dir);
    fs::create_directories(work);

    EnvironmentGuard home_guard("HOME", fake_home.string());
    EnvironmentGuard dir_guard("LOOM_CONFIG_DIR", config_dir.string());
    CurrentPathGuard cwd_guard(work);

    EXPECT_EQ(loom::constants::paths::config_home_write(), config_dir);

    loom::core::McpServerConfig cfg;
    cfg.name = "u1";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.config_scope = "user";

    {
        loom::core::ConfigManager manager;
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::User, cfg)
                        .has_value());
    }
    EXPECT_TRUE(fs::exists(config_dir / "settings.json"));
    EXPECT_FALSE(fs::exists(fake_home / ".loom" / "settings.json"));

    {
        loom::core::ConfigManager reloaded;
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "u1");
    }

    fs::remove_all(root);
}


// Group 11: unwritable files aggregate into the failed list without
// aborting writable tiers; all-unwritable-present is an empty touched list
// rather than NotFound. Skipped when running as root (perms are bypassed).

TEST(McpTypes, McpRemoveAggregatesUnwritableFiles) {
    if (::getuid() == 0) {
        GTEST_SKIP() << "read-only permissions are bypassed for root";
    }
    const auto root = c13_make_temp_root("loom_mcp_c6_unwritable_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    {
        const auto ro = root / "ro";
        fs::create_directories(ro);
        const auto user_path    = ro / "user.json";
        const auto project_path = root / "project.json";
        const auto local_path   = root / "project.local.json";
        c6_write_file(user_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");
        c6_write_file(project_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");

        fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec,
                        fs::perm_options::replace);
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 1u);
        EXPECT_EQ(outcome->touched[0], project_path);
        EXPECT_EQ(outcome->failed.size(), 1u);
        EXPECT_EQ(outcome->failed[0], user_path);

        fs::permissions(ro, fs::perms::owner_all, fs::perm_options::replace);
    }

    {
        // Both files present in one read-only directory: present, zero writes.
        const auto ro2 = root / "ro2";
        fs::create_directories(ro2);
        const auto user_path    = ro2 / "user.json";
        const auto project_path = ro2 / "project.json";
        const auto local_path   = ro2 / "project.local.json";
        c6_write_file(user_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");
        c6_write_file(project_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");

        fs::permissions(ro2, fs::perms::owner_read | fs::perms::owner_exec,
                        fs::perm_options::replace);
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_TRUE(outcome->touched.empty());
        EXPECT_EQ(outcome->failed.size(), 2u);

        fs::permissions(ro2, fs::perms::owner_all, fs::perm_options::replace);
    }

    fs::remove_all(root);
}


// Group 12 (§B secret boundary): a full save to the PROJECT file omits
// user/local-only entries (which may carry Authorization headers) and
// re-emits the PROJECT FILE'S OWN value for physically-present names, never
// the shadowing higher-tier value.

TEST(McpTypes, McpProjectSaveDoesNotLeakUserLocalSecrets) {
    const auto root = c13_make_temp_root("loom_mcp_c6_secret_boundary_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "p1": {"command": "node", "args": ["p.js"]},
    "shared": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Public": "1"}
    }
  }
})JSON");
    c6_write_file(user_path, R"JSON({
  "mcpServers": {
    "secret-srv": {
      "type": "http",
      "url": "https://secret.example.com/mcp",
      "headers": {"Authorization": "Bearer user-secret"}
    },
    "shared": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"Authorization": "Bearer user-shadow-secret"}
    }
  }
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {
    "localsecret": {
      "type": "http",
      "url": "https://local.example.com/mcp",
      "headers": {"Authorization": "Bearer local-secret"}
    }
  }
})JSON");

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        // The overlay carries the secret-bearing user/local entries.
        ASSERT_TRUE(manager.save().has_value());
    }

    const std::string rewritten = c6_read_file(project_path);
    auto doc = loom::utils::json::parse(rewritten);
    ASSERT_TRUE(doc.has_value());
    const auto servers = doc->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto keys = c6_json_keys(servers);
    EXPECT_EQ(std::set<std::string>(keys.begin(), keys.end()),
              (std::set<std::string>{"p1", "shared"}));
    // No user/local names and no secret material leak into the tracked file.
    EXPECT_EQ(rewritten.find("secret-srv"), std::string::npos);
    EXPECT_EQ(rewritten.find("localsecret"), std::string::npos);
    EXPECT_EQ(rewritten.find("Bearer"), std::string::npos);
    EXPECT_EQ(rewritten.find("Authorization"), std::string::npos);
    // The physically-present shared entry keeps the PROJECT file's own value.
    const auto shared = servers.get("shared");
    ASSERT_TRUE(shared.is_obj());
    EXPECT_EQ(shared.get("headers").get("X-Public").as_str(), std::string_view("1"));
    EXPECT_FALSE(shared.get("headers").has("Authorization"));
    EXPECT_EQ(servers.get("p1").get("command").as_str(), std::string_view("node"));

    fs::remove_all(root);
}


// B1 (review followup): mutators must keep in-memory bookkeeping and the
// merged vector consistent with disk, so a full save on the SAME
// ConfigManager instance (no reload) cannot resurrect a removed entry or
// misapply the §B filter, and an upsert stays coherent through a save.

TEST(McpTypes, McpMutationsStayCoherentForSameInstanceSave) {
    const auto root = c13_make_temp_root("loom_mcp_c6_b1_");
    // Pin CWD to the git-ancestry-clean temp root so the walk-up
    // gitignore appender never reaches a real work tree (e.g. /tmp/.git).
    CurrentPathGuard cwd_guard(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "p1": {"command": "node", "args": ["p.js"]},
    "p2": {"command": "node", "args": ["p2.js"]}
  }
})JSON");

    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_EQ(manager.settings().mcp_servers.size(), 2u);

        // Remove a PROJECT entry and immediately full-save on the SAME
        // instance (the old bug rewrote the stale merged entry back).
        auto removed = manager.remove_mcp_server(
            "p1", loom::core::McpStorageScope::Project);
        ASSERT_TRUE(removed.has_value());
        EXPECT_EQ(removed->touched.size(), 1u);
        EXPECT_TRUE(std::ranges::none_of(manager.settings().mcp_servers,
            [](const auto& s) { return s.name == "p1"; }));
        ASSERT_TRUE(manager.save().has_value());

        const std::string text = c6_read_file(project_path);
        EXPECT_EQ(text.find("p1"), std::string::npos);
        auto reparsed = loom::utils::json::parse(text);
        ASSERT_TRUE(reparsed.has_value());
        const auto servers = reparsed->root().get("mcpServers");
        EXPECT_FALSE(servers.has("p1"));
        EXPECT_TRUE(servers.has("p2"));
    }

    // A FRESH instance sees the same state.
    {
        loom::core::ConfigManager reloaded(user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        std::set<std::string> names;
        for (const auto& s : reloaded.settings().mcp_servers) names.insert(s.name);
        EXPECT_EQ(names, (std::set<std::string>{"p2"}));
    }

    // Upsert -> same-instance save coherence: new local entry must not be
    // copied down into the project file by the subsequent save (§B uses the
    // updated bookkeeping), and a project upsert round-trips.
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());

        loom::core::McpServerConfig local_cfg;
        local_cfg.name = "l1";
        local_cfg.transport = "stdio";
        local_cfg.command = "node";
        local_cfg.args = {"l.js"};
        local_cfg.config_scope = "local";
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Local, local_cfg)
                        .has_value());
        ASSERT_EQ(manager.settings().mcp_servers.size(), 2u);  // p2, l1
        ASSERT_TRUE(manager.mcp_server_owner("l1").has_value());
        EXPECT_EQ(*manager.mcp_server_owner("l1"), loom::core::McpStorageScope::Local);

        loom::core::McpServerConfig project_cfg;
        project_cfg.name = "p3";
        project_cfg.transport = "stdio";
        project_cfg.command = "node";
        project_cfg.args = {"p3.js"};
        project_cfg.config_scope = "project";
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Project, project_cfg)
                        .has_value());

        ASSERT_TRUE(manager.save().has_value());
        const std::string text = c6_read_file(project_path);
        EXPECT_EQ(text.find("l1"), std::string::npos);  // not copied down
        auto reparsed = loom::utils::json::parse(text);
        ASSERT_TRUE(reparsed.has_value());
        const auto servers = reparsed->root().get("mcpServers");
        EXPECT_TRUE(servers.has("p2"));
        EXPECT_TRUE(servers.has("p3"));

        // A lower-tier upsert while a higher tier shadows the name keeps the
        // higher value effective and the higher owner.
        loom::core::McpServerConfig shadow;
        shadow.name = "l1";
        shadow.transport = "stdio";
        shadow.command = "node";
        shadow.args = {"project-shadow.js"};
        shadow.config_scope = "project";
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Project, shadow)
                        .has_value());
        auto effective = std::ranges::find(manager.settings().mcp_servers, "l1",
                                          [](const auto& s) { return s.name; });
        ASSERT_NE(effective, manager.settings().mcp_servers.end());
        EXPECT_EQ(effective->args, (std::vector<std::string>{"l.js"}));  // local wins
        EXPECT_EQ(*manager.mcp_server_owner("l1"), loom::core::McpStorageScope::Local);
    }

    fs::remove_all(root);
}


// §A followup: a zero-length or all-whitespace user/local file behaves like
// a MISSING file — no warning state, zero entries, and an upsert creates it
// fresh — while genuine garbage keeps the rejection policy.

TEST(McpTypes, McpBlankUserLocalFilesTreatedAsMissing) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_blank_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // --- §A asymmetry: project stays a HARD failure even when blank or
    // garbage; only user/local get the soft policy. -----------------------
    { std::ofstream(project_path) << "   \n"; }  // blank project file
    {
        loom::core::ConfigManager m(user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }
    { std::ofstream(project_path) << "[1, 2]\n"; }  // valid JSON, wrong root
    {
        loom::core::ConfigManager m(user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }

    // --- Blank user/local: tolerated like a missing file, NO warning, and a
    // subsequent upsert creates the file fresh. ---------------------------
    c6_write_file(project_path, R"JSON({"mcpServers": {"g1": {"command": "node"}}})JSON");
    { std::ofstream(user_path) << ""; }
    { std::ofstream(local_path) << "  \n\t \n"; }
    {
        testing::internal::CaptureStderr();
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::string warnings = testing::internal::GetCapturedStderr();
        EXPECT_EQ(warnings.find("not valid JSON"), std::string::npos);
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(manager.settings().mcp_servers[0].name, "g1");

        loom::core::McpServerConfig cfg;
        cfg.name = "u1";
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.config_scope = "user";
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::User, cfg).has_value());
        auto user_doc = loom::utils::json::parse_file(user_path);
        ASSERT_TRUE(user_doc.has_value());
        EXPECT_TRUE(user_doc->root().get("mcpServers").has("u1"));
    }

    // --- Genuine garbage in user/local is DISTINCT from blank: the tier is
    // skipped but a warning names each bad path, and lower tiers load. ----
    fs::remove(user_path);
    fs::remove(local_path);
    { std::ofstream(user_path) << "FOO=bar\nBAZ=qux\n"; }
    { std::ofstream(local_path) << "[1, 2]\n"; }
    {
        testing::internal::CaptureStderr();
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::string warnings = testing::internal::GetCapturedStderr();
        EXPECT_NE(warnings.find(user_path.string()), std::string::npos);
        EXPECT_NE(warnings.find(local_path.string()), std::string::npos);
        EXPECT_EQ(c6_count_occurrences(warnings, "not valid JSON"), 2u);
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(manager.settings().mcp_servers[0].name, "g1");
    }

    fs::remove_all(root);
}


// Mode/gitignore follow-up: tmp+rename must preserve a pre-existing
// non-local file's mode (pre-C6 in-place ofstream kept the inode mode),
// while every LOCAL patch forces 0600 and gitignores the local basename
// even when the local file pre-existed non-empty.

TEST(McpTypes, McpPatchPreservesFileModesAndProtectsPreExistingLocal) {
    if (::getuid() == 0) {
        GTEST_SKIP() << "file mode restrictions are bypassed for root";
    }
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_modes_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    // c13e: gitignore appends require a work tree.
    ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);
    const auto user_path    = root / "user.json";
    const auto project_path = root / ".loom" / "settings.json";
    const auto local_path   = root / ".loom" / "settings.local.json";

    auto make_cfg = [](std::string name) {
        loom::core::McpServerConfig cfg;
        cfg.name = std::move(name);
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.args = {"a.js"};
        return cfg;
    };
    auto mode_of = [](const fs::path& path) {
        std::error_code ec;
        return fs::status(path, ec).permissions() & fs::perms::mask;
    };

    // Pre-existing 0600 project file keeps 0600 across upsert + disable.
    c6_write_file(project_path, R"JSON({
  "mcpServers": {"p1": {"command": "node"}}
})JSON");
    fs::permissions(project_path, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Project,
                                              make_cfg("p2")).has_value());
        EXPECT_EQ(mode_of(project_path),
                  fs::perms::owner_read | fs::perms::owner_write);
        ASSERT_TRUE(manager.set_mcp_server_disabled(
            "p1", true, loom::core::McpStorageScope::Project).has_value());
        EXPECT_EQ(mode_of(project_path),
                  fs::perms::owner_read | fs::perms::owner_write);
    }

    // Pre-existing 0640 user file keeps 0640 across an upsert.
    c6_write_file(user_path, R"JSON({
  "mcpServers": {"u1": {"command": "node"}}
})JSON");
    fs::permissions(user_path,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read,
                    fs::perm_options::replace);
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::User,
                                              make_cfg("u2")).has_value());
        EXPECT_EQ(mode_of(user_path),
                  fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read);
    }

    // A hand-created, non-empty LOCAL file with a server entry is still
    // gitignored on the next patch, and forced to 0600.
    c6_write_file(local_path, R"JSON({
  "mcpServers": {"l1": {"command": "node"}}
})JSON");
    fs::permissions(local_path,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::others_read,
                    fs::perm_options::replace);
    // c13d: the earlier PROJECT-tier writes already ignore the lock
    // sibling, but the local data basename is not ignored yet.
    if (fs::exists(root / ".gitignore")) {
        const auto prior = c6_read_file(root / ".gitignore");
        EXPECT_EQ(prior.find("settings.local.json"), std::string::npos);
        EXPECT_NE(prior.find("settings.json.lock"), std::string::npos);
    }
    {
        loom::core::ConfigManager manager(user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(loom::core::McpStorageScope::Local,
                                              make_cfg("l2")).has_value());
    }
    ASSERT_TRUE(fs::exists(root / ".gitignore"));
    EXPECT_NE(c6_read_file(root / ".gitignore").find("settings.local.json"),
              std::string::npos);
    EXPECT_NE(c6_read_file(root / ".gitignore").find("settings.local.json.lock"),
              std::string::npos);
    EXPECT_EQ(mode_of(local_path),
              fs::perms::owner_read | fs::perms::owner_write);
    // Both entries survive (pre-existing local content is not overwritten).
    auto local_doc = loom::utils::json::parse_file(local_path);
    ASSERT_TRUE(local_doc.has_value());
    EXPECT_TRUE(local_doc->root().get("mcpServers").has("l1"));
    EXPECT_TRUE(local_doc->root().get("mcpServers").has("l2"));

    fs::remove_all(root);
}

// End of file
