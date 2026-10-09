/// @file test_config.cpp
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

[[nodiscard]] std::set<fs::path>
c13_gitignores_on_chain(const fs::path& root, const fs::path& base) {
    std::set<fs::path> found;
    std::error_code ec;
    for (auto d = root; ; d = d.parent_path()) {
        if (fs::exists(d / ".gitignore", ec)) found.insert(d / ".gitignore");
        ec.clear();
        if (d == base || d.parent_path() == d) break;
    }
    return found;
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

struct EnvironmentUnsetGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvironmentUnsetGuard(std::string key) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        unsetenv(name.c_str());
    }

    ~EnvironmentUnsetGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

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

struct C13Paths {
    fs::path root;
    fs::path base;  // The actually-selected ancestry-clean base.
    fs::path user_path;
    fs::path project_path;
    fs::path local_path;

    explicit C13Paths(std::string_view tag) {
        // GTEST_SKIP() cannot run from a constructor (it expands to a
        // `return <void-expr>`), so a ctor without any clean base falls back
        // to the system temp dir rather than skipping; in practice a clean
        // base always exists (/dev/shm on Linux, clean /tmp on mac/CI), and
        // the PlainDirectoryWritesNoGitignore semantic pin performs the
        // explicit skip in void test context.
        static std::atomic<unsigned> counter{0};
        base = c13_clean_temp_base().value_or(fs::temp_directory_path());
        const auto suffix =
            std::chrono::system_clock::now().time_since_epoch().count();
        // Anchored under a git-ancestry-clean base so project/local writes
        // and full saves (whose .gitignore appender walks up from cwd) can
        // never reach a real work tree such as a stray /tmp/.git.
        root = base /
               (std::string("loom_c13_") + std::string(tag) + "_" +
                std::to_string(::getpid()) + "_" +
                std::to_string(counter.fetch_add(1,
                    std::memory_order_relaxed)) + "_" +
                std::to_string(suffix));
        fs::create_directories(root);
        user_path    = root / "user.json";
        project_path = root / "project.json";
        local_path   = root / "project.local.json";
    }
    ~C13Paths() { std::error_code ec; fs::remove_all(root, ec); }

    [[nodiscard]] loom::core::ConfigManager manager() const {
        return loom::core::ConfigManager(user_path, project_path, local_path);
    }
};

void c13_write_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

struct C13Json {
    loom::utils::json::JsonDoc doc;
    loom::utils::json::JsonVal root;

    explicit C13Json(std::string_view text) {
        if (auto parsed = loom::utils::json::parse(text)) {
            doc = std::move(*parsed);
            root = doc.root();
        }
    }
    operator const loom::utils::json::JsonVal&() const noexcept { return root; }
};

[[nodiscard]] C13Json c13_parse(std::string_view text) {
    return C13Json(text);
}

struct MigrationPaths {
    fs::path root;
    fs::path home;      // fake $HOME
    fs::path user_dir;  // <home>/.loom
    fs::path work;      // fake cwd

    explicit MigrationPaths(std::string_view tag)
        : root(c13_make_temp_root(
              std::string("loom_migrate_") + std::string(tag)))
        , home(root / "home")
        , user_dir(home / ".loom")
        , work(root / "work") {
        fs::create_directories(user_dir);
        fs::create_directories(work);
    }
    ~MigrationPaths() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    [[nodiscard]] fs::path user_config() const {
        return user_dir / "config.json";
    }
    [[nodiscard]] fs::path user_settings() const {
        return user_dir / "settings.json";
    }
    [[nodiscard]] fs::path user_config_bak() const {
        return user_dir / "config.json.bak";
    }
};

struct C13GitRepo {
    fs::path root;
    fs::path loom;
    fs::path outside;
    std::unique_ptr<CurrentPathGuard> cwd_guard;
    bool available = false;

    explicit C13GitRepo(std::string_view tag)
        : root(fs::temp_directory_path() /
               (std::string("loom_c13_git_") + std::string(tag) + "_" +
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()))) {
        if (std::system("git --version >/dev/null 2>&1") != 0) return;
        fs::create_directories(root);
        loom = root / ".loom";
        outside = root / "outside";
        fs::create_directories(loom);
        fs::create_directories(outside);
        if (std::system(("git -C \"" + root.string() +
                         "\" init -q --initial-branch main")
                            .c_str()) != 0) {
            return;
        }
        std::system(("git -C \"" + root.string() +
                     "\" config user.email t@example.invalid").c_str());
        std::system(("git -C \"" + root.string() +
                     "\" config user.name c13d").c_str());
        // Enter the repo only after it exists.
        cwd_guard = std::make_unique<CurrentPathGuard>(root);
        available = true;
    }
    ~C13GitRepo() {
        // Restore cwd before removing the tree.
        cwd_guard.reset();
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void git(std::string_view cmd) const {
        ASSERT_EQ(std::system(std::string(cmd).c_str()), 0) << cmd;
    }

    [[nodiscard]] std::string porcelain() const {
        const auto out = root / "porcelain.txt";
        const std::string cmd =
            "git status --porcelain=v1 > " + out.string() + " 2>/dev/null";
        std::system(cmd.c_str());
        std::ifstream f(out);
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }

    [[nodiscard]] loom::core::ConfigManager manager() const {
        return loom::core::ConfigManager(outside / "user.json",
                                       loom / "settings.json",
                                       loom / "settings.local.json");
    }
};

bool c13_git_available() {
    return std::system("git --version >/dev/null 2>&1") == 0;
}

} // namespace

TEST(ConfigManager, PersistsMcpServerSettings) {
    const auto root = c13_make_temp_root("loom_config_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    loom::core::ConfigManager manager(root / "project.json");
    auto& settings = manager.settings_mut();
    settings.mcp_servers.push_back(loom::core::McpServerConfig{
        .name = "echo",
        .command = "node",
        .args = {"server.js", "--flag"},
        .env = {{"FOO", "bar"}},
    });

    ASSERT_TRUE(manager.save().has_value());

    loom::core::ConfigManager loaded(root / "project.json");
    ASSERT_TRUE(loaded.load().has_value());
    ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
    EXPECT_EQ(loaded.settings().mcp_servers.front().name, "echo");
    EXPECT_EQ(loaded.settings().mcp_servers.front().command, "node");
    ASSERT_EQ(loaded.settings().mcp_servers.front().args.size(), 2u);
    EXPECT_EQ(loaded.settings().mcp_servers.front().args[0], "server.js");
    EXPECT_EQ(loaded.settings().mcp_servers.front().args[1], "--flag");
	EXPECT_EQ(loaded.settings().mcp_servers.front().env.at("FOO"), "bar");

	fs::remove_all(root);
}

TEST(ConfigManager, PreservesRemoteMcpServerAuthSettings) {
    const auto root = c13_make_temp_root("loom_remote_mcp_config_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    loom::core::ConfigManager manager(root / "project.json");
    auto& settings = manager.settings_mut();
    settings.mcp_servers.push_back(loom::core::McpServerConfig{
        .name = "remote",
        .command = {},
        .args = {},
        .env = {},
        .transport = "http",
        .url = "https://mcp.example.com/mcp",
        .headers = {{"X-Test", "present"}},
        .headers_helper = "node headers.js",
        .oauth = loom::core::McpOAuthConfig{
            .auth_server_metadata_url = "https://auth.example.com/.well-known/oauth-authorization-server",
            .callback_port = 19485,
            .client_id = "client-1",
            .xaa = true,
        },
    });

    ASSERT_TRUE(manager.save().has_value());

    loom::core::ConfigManager loaded(root / "project.json");
    ASSERT_TRUE(loaded.load().has_value());
    ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
    const auto& server = loaded.settings().mcp_servers.front();
    EXPECT_EQ(server.name, "remote");
    EXPECT_EQ(server.transport, "http");
    ASSERT_TRUE(server.url.has_value());
    EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
    EXPECT_EQ(server.headers.at("X-Test"), "present");
    ASSERT_TRUE(server.headers_helper.has_value());
    EXPECT_EQ(*server.headers_helper, "node headers.js");
    ASSERT_TRUE(server.oauth.has_value());
    ASSERT_TRUE(server.oauth->auth_server_metadata_url.has_value());
    EXPECT_EQ(*server.oauth->auth_server_metadata_url, "https://auth.example.com/.well-known/oauth-authorization-server");
    ASSERT_TRUE(server.oauth->callback_port.has_value());
    EXPECT_EQ(*server.oauth->callback_port, 19485);
    ASSERT_TRUE(server.oauth->client_id.has_value());
    EXPECT_EQ(*server.oauth->client_id, "client-1");
    EXPECT_TRUE(server.oauth->xaa);

    fs::remove_all(root);
}


// ===========================================================================
// RFC-0001 B followup c13b — structured user settings on ConfigManager.
// ===========================================================================


// All 7 writable kinds round-trip through a quiet reload and a fresh
// manager, with the canonical value token and repaired=null on a valid file.

TEST(ConfigManagerUserSettings, SevenWritableKindsRoundTrip) {
    C13Paths p("seven");
    {
        auto m = p.manager();
        auto setv = [&](std::string_view key, std::string_view json) {
            auto value = c13_parse(json);
            auto out = m.set_user_setting(key, value);
            ASSERT_TRUE(out.has_value()) << out.error().message;
            EXPECT_FALSE(out->repaired.has_value());
        };
        setv("model", R"("  my-model  ")");
        setv("maxOutputTokens", "4096");
        setv("temperature", "0.5");
        setv("extendedThinking", "true");
        setv("thinkingBudget", "2048");
        setv("contextWindowSize", "100000");
        setv("maxRetries", "0");
    }
    auto fresh = p.manager();
    ASSERT_TRUE(fresh.load().has_value());
    const auto& s = fresh.settings();
    EXPECT_EQ(s.model.default_model, "my-model");
    EXPECT_EQ(s.model.max_output_tokens, 4096u);
    EXPECT_DOUBLE_EQ(*s.model.temperature, 0.5);
    EXPECT_TRUE(s.model.extended_thinking);
    EXPECT_EQ(*s.model.thinking_budget, 2048u);
    EXPECT_EQ(s.model.context_window_size, 100000u);
    EXPECT_EQ(s.network.max_retries, 0u);

    // Native JSON bool/number inputs are accepted and normalized.
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out1 = m.set_user_setting("extendedThinking", c13_parse("false"));
        ASSERT_TRUE(out1.has_value());
        auto out2 = m.set_user_setting("temperature", c13_parse("0.25"));
        ASSERT_TRUE(out2.has_value());
        auto out3 = m.set_user_setting("maxRetries", c13_parse("5"));
        ASSERT_TRUE(out3.has_value()) << out3.error().message;
    }
    auto reloaded = p.manager();
    ASSERT_TRUE(reloaded.load().has_value());
    EXPECT_FALSE(reloaded.settings().model.extended_thinking);
    EXPECT_DOUBLE_EQ(*reloaded.settings().model.temperature, 0.25);
    EXPECT_EQ(reloaded.settings().network.max_retries, 5u);
}


// Temperature: [0,1] bounds, text/number inputs, and null-clear (user tier
// only — a lower-tier value still wins the merge).

TEST(ConfigManagerUserSettings, TemperatureBoundsAndClear) {
    C13Paths p("temp");
    c13_write_file(p.project_path, R"JSON({"temperature":0.25})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    for (const auto* bad : {"-0.01", "1.01", "2", "\"hot\"", "true"}) {
        auto out = m.set_user_setting("temperature", c13_parse(bad));
        EXPECT_FALSE(out.has_value()) << bad;
    }
    for (const auto* good : {"0", "1", "0.75", "\"0.1\""}) {
        auto out = m.set_user_setting("temperature", c13_parse(good));
        EXPECT_TRUE(out.has_value()) << good << ": "
            << (out ? "" : out.error().message);
    }
    // Explicit null clears the USER leaf: the file stores null and the
    // effective value falls through to the project 0.25.
    {
        auto out = m.set_user_setting("temperature", c13_parse("null"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->value_token, "null");
    }
    {
        std::ifstream in(p.user_path);
        std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("\"temperature\": null"), std::string::npos);
    }
    auto after_mgr = p.manager();
    ASSERT_TRUE(after_mgr.load().has_value());
    ASSERT_TRUE(after_mgr.settings().model.temperature.has_value());
    EXPECT_DOUBLE_EQ(*after_mgr.settings().model.temperature, 0.25);
}


// thinking_budget: null/0 clear, otherwise the API 1024 minimum.

TEST(ConfigManagerUserSettings, ThinkingBudgetBoundsAndClear) {
    C13Paths p("budget");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    EXPECT_FALSE(m.set_user_setting("thinkingBudget", c13_parse("512")).has_value());
    EXPECT_FALSE(m.set_user_setting("thinkingBudget", c13_parse("\"abc\"")).has_value());
    EXPECT_FALSE(m.set_user_setting("thinkingBudget", c13_parse("-1")).has_value());
    ASSERT_TRUE(m.set_user_setting("thinkingBudget", c13_parse("1024")).has_value());
    {
        auto at_min = p.manager();
        ASSERT_TRUE(at_min.load().has_value());
        EXPECT_EQ(*at_min.settings().model.thinking_budget, 1024u);
    }
    // 0 maps to a null clear; the value is removed from the user file.
    ASSERT_TRUE(m.set_user_setting("thinkingBudget", c13_parse("0")).has_value());
    ASSERT_TRUE(m.set_user_setting("thinkingBudget", c13_parse("null")).has_value());
    auto after_mgr = p.manager();
    ASSERT_TRUE(after_mgr.load().has_value());
    EXPECT_FALSE(after_mgr.settings().model.thinking_budget.has_value());
}


// Malformed / out-of-range values are rejected before any write.

TEST(ConfigManagerUserSettings, MalformedValuesRejected) {
    C13Paths p("malformed");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto rejected = [&](std::string_view key, std::string_view json) {
        auto out = m.set_user_setting(key, c13_parse(json));
        EXPECT_FALSE(out.has_value()) << key << " <- " << json;
    };
    rejected("model", R"("")");
    rejected("model", R"("   ")");
    rejected("model", "42");
    rejected("maxOutputTokens", "\"abc\"");
    rejected("maxOutputTokens", "-1");
    rejected("maxOutputTokens", "1.5");
    rejected("maxOutputTokens", "4294967296"); // uint32 max + 1
    rejected("maxOutputTokens", "\"18446744073709551616\"");
    rejected("maxOutputTokens", "\"-5\"");
    rejected("contextWindowSize", "0");        // positive uint
    rejected("extendedThinking", "\"yes\"");    // exact true|false only
    rejected("extendedThinking", "1");
    rejected("maxRetries", "\"1x\"");
    rejected("temperature", "{}");

    // No file was created by the failed writes.
    EXPECT_FALSE(fs::exists(p.user_path));

    // max_retries accepts 0; the positive keys do not.
    ASSERT_TRUE(m.set_user_setting("maxRetries", c13_parse("0"))
                    .has_value());
}


// Read-only / blocked / unknown keys each get their terminal error, and no
// write happens.

TEST(ConfigManagerUserSettings, ReadonlyBlockedUnknownRejected) {
    C13Paths p("blocked");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto ro = m.set_user_setting("theme", c13_parse(R"("dark")"));
    ASSERT_FALSE(ro.has_value());
    EXPECT_NE(ro.error().message.find("not writable through this tool"),
              std::string::npos);
    EXPECT_NE(ro.error().message.find("no runtime component consumes"),
              std::string::npos);

    const std::array<std::pair<const char*, const char*>, 9> blocked_cases = {{
        {"apiKey", "LOOM_API_KEY"},
        {"baseUrl", "LOOM_BASE_URL"},
        {"proxy", "HTTPS_PROXY"},
        {"verifySsl", "TLS"},
        {"permissions.deny_rules", "permission"},
        {"systemPrompt", "session"},
        {"customInstructions", "session"},
        {"mcpServers", "loom mcp"},
        {"xaaIdp", "/mcp xaa"},
    }};
    for (const auto& [key, hint] : blocked_cases) {
        auto out = m.set_user_setting(key, c13_parse(R"("x")"));
        ASSERT_FALSE(out.has_value()) << key;
        EXPECT_NE(out.error().message.find(hint), std::string::npos)
            << key << ": " << out.error().message;
    }

    auto unknown = m.set_user_setting("totally.bogus", c13_parse("1"));
    ASSERT_FALSE(unknown.has_value());
    EXPECT_NE(unknown.error().message.find("Unknown configuration key"),
              std::string::npos);
    EXPECT_NE(unknown.error().message.find("action=list"), std::string::npos);

    EXPECT_FALSE(fs::exists(p.user_path));
}


// Read-only keys are projected with writable=false / consumes=[], and the
// list payload carries the closed sets plus enum values for display.theme.

TEST(ConfigManagerUserSettings, ReadOnlyProjectionAndListShape) {
    C13Paths p("projection");
    c13_write_file(p.user_path, R"JSON({
      "theme": "dark", "lineWidth": 120, "compactMode": true,
      "timeoutSeconds": 45,
      "permissions": {"allow_network": false}
    })JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto theme = m.agent_setting_value_json("theme");
    ASSERT_TRUE(theme.has_value());
    auto theme_val = c13_parse(*theme);
    EXPECT_EQ(theme_val.root.get("writable").as_bool(), false);
    EXPECT_TRUE(theme_val.root.get("consumes").is_arr());
    EXPECT_EQ(theme_val.root.get("consumes").size(), 0u);
    EXPECT_EQ(std::string(theme_val.root.get("source").as_str()), "file");
    EXPECT_EQ(std::string(theme_val.root.get("value").as_str()), "dark");

    auto settings = c13_parse(m.serialize_agent_settings_json());
    EXPECT_TRUE(settings.root.get("temperature").get("value").is_null());
    ASSERT_TRUE(settings.root.get("temperature").is_obj());
    EXPECT_EQ(settings.root.get("lineWidth").get("value").as_int(), 120);
    EXPECT_EQ(settings.root.get("timeoutSeconds").get("value").as_int(), 45);
    EXPECT_EQ(settings.root.get("permissions").get("allow_network").get("value").as_bool(), false);

    auto list = c13_parse(loom::core::ConfigManager::serialize_user_setting_specs_json());
    EXPECT_EQ(list.root.get("writable").size(), 7u);
    EXPECT_EQ(list.root.get("read_only").size(), 9u);
    EXPECT_GE(list.root.get("blocked").size(), 9u);
    // Enumeration metadata survives into the list payload.
    bool found_theme = false;
    list.root.get("read_only").iter([&](auto item) {
        if (std::string(item.get("key").as_str()) == "theme") {
            found_theme = true;
            EXPECT_EQ(item.get("enum_values").size(), 3u);
        }
    });
    EXPECT_TRUE(found_theme);
    auto null_keys = list.root.get("null_clear_keys");
    ASSERT_EQ(null_keys.size(), 2u);
    EXPECT_EQ(std::string(null_keys.at(0).as_str()), "temperature");
    EXPECT_EQ(std::string(null_keys.at(1).as_str()), "thinkingBudget");
}


// Patching one leaf preserves sibling model/display keys, unknown top-level
// keys, and the mcpServers section byte-for-byte structurally.

TEST(ConfigManagerUserSettings, SiblingUnknownAndMcpPreserved) {
    C13Paths p("preserve");
    c13_write_file(p.user_path, R"JSON({
  "model": "kept-model",
  "contextWindowSize": 123456,
  "unknown_future_flag": true,
  "theme": "light",
  "mcpServers": {"srv": {"command": "node", "args": ["x.js"]}},
  "totally_unknown_section": {"a": [1, 2, 3]}
})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.set_user_setting("maxRetries", c13_parse("7"))
                    .has_value());

    auto doc = loom::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    EXPECT_EQ(root.get("model").as_str(),
              std::string_view("kept-model"));
    EXPECT_EQ(root.get("contextWindowSize").as_int(), 123456);
    EXPECT_EQ(root.get("unknown_future_flag").as_bool(), true);
    EXPECT_EQ(root.get("theme").as_str(),
              std::string_view("light"));
    EXPECT_EQ(root.get("mcpServers").get("srv").get("command").as_str(),
              std::string_view("node"));
    EXPECT_EQ(root.get("totally_unknown_section").get("a").at(1).as_int(), 2);
    EXPECT_EQ(root.get("maxRetries").as_int(), 7);
}


// Salvage tri-state: valid object untouched; leading object + trailing junk
// dropped; leading non-object and malformed objects replaced wholesale.

TEST(ConfigManagerUserSettings, SalvageTriState) {
    C13Paths p("salvage");

    // 1. strict-valid object → Untouched (repaired null), keys preserved.
    c13_write_file(p.user_path, R"JSON({"model": "a", "x": 1})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("2"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("model").as_str(),
                  std::string_view("a"));
        EXPECT_EQ(root.root.get("x").as_int(), 1);
    }

    // 2. complete leading OBJECT + trailing junk → trailing_junk_dropped.
    c13_write_file(p.user_path,
                  R"JSON({"model": "b"}} TRAILING GARBAGE)JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("3"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "trailing_junk_dropped");
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("model").as_str(),
                  std::string_view("b"));
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 3);
    }

    // 3. leading complete non-object [1,2] + junk → replaced_unparseable.
    c13_write_file(p.user_path, "[1,2] GARBAGE");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("4"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 4);
    }

    // 4. trailing-comma malformed object → replaced_unparseable.
    c13_write_file(p.user_path, R"JSON({"model": "c",}})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("5"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
    }

    // 5. pure junk → replaced_unparseable.
    c13_write_file(p.user_path, "FOO=bar\nBAZ=qux\n");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("6"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->repaired.value_or(""), "replaced_unparseable");
    }
}


// Duplicate keys on the patched leaf are canonicalized to one; the write is
// valid JSON with a single key (yyjson obj_put replaces the first twin).

TEST(ConfigManagerUserSettings, DuplicateKeyCanonicalizedOnPatch) {
    C13Paths p("dup");
    c13_write_file(p.user_path,
        R"JSON({"model": "one", "model": "two"}}junk!)JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("model", c13_parse(R"("final")"));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->repaired.value_or(""), "trailing_junk_dropped");

    std::ifstream f(p.user_path);
    const std::string bytes((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    // Exactly one canonical leaf spelling survives.
    EXPECT_EQ(c6_count_occurrences(bytes, "\"model\""), 1u);
    EXPECT_EQ(bytes.find("\"one\""), std::string::npos);
    EXPECT_EQ(bytes.find("\"two\""), std::string::npos);
    auto root = c13_parse(bytes);
    EXPECT_EQ(root.root.get("model").as_str(),
              std::string_view("final"));
}


// Blank/missing user files start from {} and report repaired=null.

TEST(ConfigManagerUserSettings, BlankAndMissingFileFreshStart) {
    C13Paths p("blank");
    {
        std::ofstream(p.user_path) << "  \n\t \n";
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
    }
    fs::remove(p.user_path);
    {
        auto m = p.manager();
        auto out = m.set_user_setting("maxRetries", c13_parse("2"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 2);
    }
}


// A pre-existing 0600 user file keeps 0600 across the tmp+rename patch.

TEST(ConfigManagerUserSettings, Mode0600Preserved) {
    if (::getuid() == 0) GTEST_SKIP() << "modes are bypassed for root";
    C13Paths p("mode");
    c13_write_file(p.user_path, R"JSON({"maxRetries": 1}})JSON");
    fs::permissions(p.user_path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.set_user_setting("maxRetries", c13_parse("9"))
                    .has_value());
    const auto mode = fs::status(p.user_path).permissions() & fs::perms::mask;
    EXPECT_EQ(mode, fs::perms::owner_read | fs::perms::owner_write);
}


// $LOOM_CONFIG_DIR routes the write; only that one file is touched.

TEST(ConfigManagerUserSettings, ConfigDirRoutingOnlyUserTouched) {
    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
                      ("loom_c13_cfgdir_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto fake_home = root / "home";
    const auto cfg_dir = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(fake_home);
    fs::create_directories(cfg_dir);
    fs::create_directories(work);
    EnvironmentGuard home_guard("HOME", fake_home.string());
    EnvironmentGuard dir_guard("LOOM_CONFIG_DIR", cfg_dir.string());
    CurrentPathGuard cwd_guard(work);

    {
        loom::core::ConfigManager m;
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("4"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->path, cfg_dir / "settings.json");
    }
    EXPECT_TRUE(fs::exists(cfg_dir / "settings.json"));
    EXPECT_FALSE(fs::exists(fake_home / ".loom"));
    EXPECT_FALSE(fs::exists(work / ".loom"));
    std::error_code ec;
    fs::remove_all(root, ec);
}


// Provenance: source env/file/default, the LOOM_MODEL shadow bit, and
// presence-only secret projection (credential bytes never serialized).

TEST(ConfigManagerUserSettings, ProvenanceShadowAndSecretPresence) {
    // Presence/provenance assertions are sensitive to ambient credential
    // env vars (including LOOM_AUTH_TOKEN, which counts as presence
    // for network.api_key); start hermetic. Inner EnvironmentGuards below
    // restore into the unset state, and these restore the shell at exit.
    EnvironmentUnsetGuard g_api("LOOM_API_KEY");
    EnvironmentUnsetGuard g_auth("LOOM_AUTH_TOKEN");
    EnvironmentUnsetGuard g_base("LOOM_BASE_URL");
    EnvironmentUnsetGuard g_http_proxy("HTTP_PROXY");
    EnvironmentUnsetGuard g_https_proxy("HTTPS_PROXY");

    C13Paths p("provenance");
    c13_write_file(p.user_path, R"JSON({
      "model": "file-model", "maxOutputTokens": 111,
      "apiKey": "SECRET-zx9w87-traceable-key",
      "baseUrl": "https://SECRET-zx9w87.example.invalid/api"
    })JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto settings = c13_parse(m.serialize_agent_settings_json());
        EXPECT_EQ(std::string(settings.root.get("model")
                                  .get("source").as_str()), "file");
        EXPECT_EQ(std::string(settings.root.get("maxRetries")
                                  .get("source").as_str()), "default");

        auto presence = m.agent_secret_presence_json("apiKey");
        ASSERT_TRUE(presence.has_value());
        auto pv = c13_parse(*presence);
        EXPECT_EQ(pv.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(pv.root.get("source").as_str()), "file");
        EXPECT_EQ(presence->find("SECRET-zx9w87"), std::string::npos);

        auto url = m.agent_secret_presence_json("baseUrl");
        ASSERT_TRUE(url.has_value());
        EXPECT_EQ(url->find("SECRET-zx9w87"), std::string::npos);

        auto none = m.agent_secret_presence_json("proxy");
        ASSERT_TRUE(none.has_value());
        auto nv = c13_parse(*none);
        EXPECT_EQ(nv.root.get("set").as_bool(), false);
        EXPECT_EQ(std::string(nv.root.get("source").as_str()), "none");
    }

    // Env engagement flips source + shadow; the file write still happens.
    EnvironmentGuard model_guard("LOOM_MODEL", "env-model");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto token = m.agent_setting_value_json("model");
        ASSERT_TRUE(token.has_value());
        auto tv = c13_parse(*token);
        EXPECT_EQ(std::string(tv.root.get("source").as_str()), "env");
        EXPECT_EQ(std::string(tv.root.get("env_var").as_str()), "LOOM_MODEL");
        EXPECT_EQ(std::string(tv.root.get("value").as_str()), "env-model");

        auto out = m.set_user_setting("model",
                                      c13_parse(R"("written-model")"));
        ASSERT_TRUE(out.has_value());
        EXPECT_TRUE(out->shadowed);
        EXPECT_EQ(out->shadowed_by, "LOOM_MODEL");

        // The post-write reload keeps reporting the env value/source.
        auto after = c13_parse(
            *m.agent_setting_value_json("model"));
        EXPECT_EQ(std::string(after.root.get("source").as_str()), "env");
        EXPECT_EQ(std::string(after.root.get("value").as_str()), "env-model");
        std::ifstream f(p.user_path);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("written-model"), std::string::npos);
    }

    // LOOM_API_KEY presence is env-sourced and never leaks bytes,
    // neither single-get nor get-all.
    EnvironmentGuard key_guard("LOOM_API_KEY",
                               "SECRET-zx9w87-traceable-key");
    EnvironmentGuard proxy_guard("HTTP_PROXY", "http://SECRET-zx9w87-proxy:3128");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto presence = m.agent_secret_presence_json("apiKey");
        ASSERT_TRUE(presence.has_value());
        auto pv = c13_parse(*presence);
        EXPECT_EQ(pv.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(pv.root.get("source").as_str()), "env");
        EXPECT_EQ(presence->find("SECRET-zx9w87"), std::string::npos);

        auto proxy = m.agent_secret_presence_json("proxy");
        ASSERT_TRUE(proxy.has_value());
        auto px = c13_parse(*proxy);
        EXPECT_EQ(px.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(px.root.get("source").as_str()), "env");
        EXPECT_EQ(proxy->find("SECRET-zx9w87"), std::string::npos);

        const std::string all = m.serialize_agent_settings_json();
        EXPECT_EQ(all.find("SECRET-zx9w87"), std::string::npos);
    }
}


// LOOM_MODEL adds the interactive-resolver source note.

TEST(ConfigManagerUserSettings, ModelSourceNote) {
    C13Paths p("note");
    EnvironmentGuard guard("LOOM_MODEL", "interactive-model");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto token = m.agent_setting_value_json("model");
    ASSERT_TRUE(token.has_value());
    EXPECT_NE(token->find("LOOM_MODEL"),
              std::string::npos);
}


// A section holding the wrong JSON type is replaced by an object on write.

TEST(ConfigManagerUserSettings, SectionWrongTypeReplaced) {
    C13Paths p("wrongtype");
    c13_write_file(p.user_path, R"JSON({"model":[1,2,3]})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("model",
                                  c13_parse(R"("rebuilt")"));
    ASSERT_TRUE(out.has_value());
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    ASSERT_TRUE(root.root.get("model").is_str());
    EXPECT_EQ(root.root.get("model").as_str(),
              std::string_view("rebuilt"));
}


// Quiet load emits zero stderr bytes; a salvaged write clears the
// unparseable flag within the same instance; a corrupt other tier still
// succeeds with reload_warning.

TEST(ConfigManagerUserSettings, SilentLoadSalvageClearsFlagAndReloadWarning) {
    C13Paths p("silent");
    c13_write_file(p.user_path, "GARBAGE NOT JSON\n");
    {
        auto m = p.manager();
        testing::internal::CaptureStderr();
        ASSERT_TRUE(m.load(loom::core::LoadOptions{.quiet = true}).has_value());
        const std::string captured = testing::internal::GetCapturedStderr();
        EXPECT_TRUE(captured.empty()) << captured;
        EXPECT_TRUE(m.user_tier_unparseable());

        // Audible path (CLI/core loader) keeps the warning.
        auto audible = p.manager();
        testing::internal::CaptureStderr();
        ASSERT_TRUE(audible.load().has_value());
        EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());

        // Salvaging write repairs the file and clears the flag (D4 reload).
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
        EXPECT_FALSE(m.user_tier_unparseable());
        auto fresh = p.manager();
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_FALSE(fresh.user_tier_unparseable());
        EXPECT_EQ(fresh.settings().network.max_retries, 1u);
    }

    // Corrupt PROJECT tier: the user write still succeeds; the post-write
    // quiet reload surfaces it as reload_warning without rolling back.
    c13_write_file(p.project_path, "BROKEN PROJECT JSON");
    {
        auto m = p.manager();
        // Pre-write load hard-fails on the project tier, as designed.
        EXPECT_FALSE(m.load(loom::core::LoadOptions{.quiet = true}).has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("8"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        ASSERT_TRUE(out->reload_warning.has_value());
        EXPECT_NE(out->reload_warning->find("parse"), std::string::npos);
        EXPECT_TRUE(fs::exists(p.user_path));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 8);
    }
}


// The spec table is closed: exactly 16 projected keys, 7 writable, with
// coherent section/leaf/dotted tokens.

TEST(ConfigManagerUserSettings, SpecTableIsClosed) {
    const auto specs = loom::core::ConfigManager::user_setting_specs();
    EXPECT_EQ(specs.size(), 16u);
    int writable = 0;
    for (const auto& spec : specs) {
        // Flat keys (section == "") have key == leaf; nested keys have
        // key == section + "." + leaf.
        if (spec.section.empty()) {
            EXPECT_EQ(spec.key, spec.leaf);
        } else {
            EXPECT_EQ(spec.key,
                      std::string(spec.section) + "." +
                          std::string(spec.leaf));
        }
        if (spec.writable) ++writable;
    }
    EXPECT_EQ(writable, 7);
    EXPECT_NE(loom::core::ConfigManager::find_user_setting("temperature"),
              nullptr);
    EXPECT_EQ(loom::core::ConfigManager::find_user_setting("nope.nope"),
              nullptr);
    EXPECT_TRUE(loom::core::ConfigManager::blocked_setting_message("xaaIdp")
                    .has_value());
    EXPECT_FALSE(loom::core::ConfigManager::blocked_setting_message("temperature")
                     .has_value());
}


// ===========================================================================
// RFC-0001 B followup c13c — hardened atomic writer + coercion/env/BOM nits.
// ===========================================================================

// Pre-placed settings.json.tmp symlink must never be followed: the victim is
// untouched, settings.json ends up a regular file, and the stale symlink is
// removed rather than left behind.

TEST(ConfigManagerUserSettings, AtomicWriterTmpSymlinkNeverFollowed) {
    C13Paths p("symlink");
    fs::create_directories(p.root);
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-CANARY-c13c-7788";
    { std::ofstream(victim) << kCanary; }
    // The pre-c13c writer used the fixed suffix user_path + ".tmp";
    // pre-place that exact name as a symlink to the victim.
    const auto legacy_tmp = p.user_path.parent_path() /
                            (p.user_path.filename().string() + ".tmp");
    fs::remove(legacy_tmp);
    fs::create_symlink(victim, legacy_tmp);

    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("maxRetries", c13_parse("3"));
    ASSERT_TRUE(out.has_value()) << out.error().message;

    // Victim bytes unchanged.
    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    // settings.json is a REGULAR file with the correct JSON.
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.user_path, ec).type(),
              fs::file_type::regular);
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    EXPECT_EQ(root.root.get("maxRetries").as_int(), 3);
    // No leftover attacker symlink.
    EXPECT_NE(fs::symlink_status(legacy_tmp, ec).type(),
              fs::file_type::symlink);
}


// A pre-created directory at the legacy tmp name is refused cleanly; a
// pre-created FIFO is treated as old crash debris and auto-unlinked so the
// write proceeds (c13d; regular/FIFO debris must not block post-upgrade).

TEST(ConfigManagerUserSettings, AtomicWriterTmpDirRefusedFifoCleared) {
    C13Paths p("tmpblock");
    const auto legacy_tmp = p.root /
                            (p.user_path.filename().string() + ".tmp");

    // Directory: refused, nothing clobbered.
    {
        std::error_code ec;
        fs::remove_all(legacy_tmp, ec);
        fs::create_directories(legacy_tmp, ec);
        ASSERT_FALSE(ec);
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("3"));
        EXPECT_FALSE(out.has_value());
        EXPECT_EQ(fs::symlink_status(legacy_tmp).type(),
                  fs::file_type::directory);
        fs::remove_all(legacy_tmp, ec);
    }

    // FIFO: treated as stale debris, removed, write succeeds (no blocking
    // open).
    {
        ASSERT_EQ(::mkfifo(legacy_tmp.c_str(), 0600), 0);
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("maxRetries", c13_parse("3"));
        EXPECT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(legacy_tmp));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 3);
    }
}


// 20 parallel processes hammering distinct settings on one config dir:
// every child either writes parse-valid JSON or fails cleanly, no torn
// files, no fixed-tmp "Failed to replace" collisions; the final file is
// valid and holds the last successful value for each key.

TEST(ConfigManagerUserSettings, ConcurrentSetProcessesNoCollisions) {
    C13Paths p("concurrent");
    constexpr int kProcesses = 20;
    constexpr std::array<std::string_view, 7> keys = {{
        "maxRetries",
        "maxOutputTokens",
        "contextWindowSize",
        "thinkingBudget",
        "extendedThinking",
        "temperature",
        "model",
    }};
    std::array<std::string, keys.size()> last_values{{
        "100", "20000", "300000", "4096", "true", "0.5", "concurrent-model"
    }};
    std::vector<pid_t> children;
    for (int i = 0; i < kProcesses; ++i) {
        pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            const auto& key = keys[static_cast<std::size_t>(i % keys.size())];
            auto m = p.manager();
            (void)m.load();
            const std::string value = last_values[i % keys.size()];
            const std::string payload =
                key == "model"
                    ? std::format("\"{}\"", value)
                    : value;
            auto out = m.set_user_setting(key, c13_parse(payload));
            _exit(out.has_value() ? 0 : 2);
        }
        children.push_back(pid);
    }
    int clean_failures = 0;
    for (pid_t pid : children) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        const int code = WEXITSTATUS(status);
        ASSERT_NE(code, 2) << "child reported a write-level error";
        if (code != 0) ++clean_failures;
    }
    // At least the seven keys' final writes must land (single wave, unique
    // tmp names — expected zero failures).
    EXPECT_EQ(clean_failures, 0);

    auto doc = loom::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    ASSERT_TRUE(root.is_obj());
    EXPECT_EQ(root.get("maxRetries").as_int(), 100);
    EXPECT_EQ(root.get("maxOutputTokens").as_int(), 20000);
    EXPECT_EQ(root.get("contextWindowSize").as_int(), 300000);
    EXPECT_EQ(root.get("thinkingBudget").as_int(), 4096);
    EXPECT_EQ(root.get("extendedThinking").as_bool(), true);
    EXPECT_DOUBLE_EQ(root.get("temperature").as_double(), 0.5);
    EXPECT_EQ(root.get("model").as_str(),
              std::string_view("concurrent-model"));

    // No stale tmp debris (fixed or unique-named) remains.
    for (const auto& entry : fs::directory_iterator(p.root)) {
        const auto name = entry.path().filename().string();
        EXPECT_EQ(name.find(".tmp"), std::string::npos) << name;
    }
}


// Native JSON numbers that are integral doubles within uint32 range are
// accepted; fractional/negative/overflow values rejected (D7).

TEST(ConfigManagerUserSettings, IntegralDoubleCoercion) {
    C13Paths p("intdouble");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    ASSERT_TRUE(m.set_user_setting("maxOutputTokens", c13_parse("4096.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("contextWindowSize", c13_parse("100000.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("maxRetries", c13_parse("0.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("thinkingBudget", c13_parse("2048.0"))
                    .has_value());

    for (const auto* bad : {"4096.5", "-1.0", "4294967296.0", "1e40"}) {
        EXPECT_FALSE(m.set_user_setting("maxOutputTokens",
                                        c13_parse(bad)).has_value()) << bad;
    }
    auto reloaded = p.manager();
    ASSERT_TRUE(reloaded.load().has_value());
    EXPECT_EQ(reloaded.settings().model.max_output_tokens, 4096u);
    EXPECT_EQ(reloaded.settings().model.context_window_size, 100000u);
    EXPECT_EQ(reloaded.settings().network.max_retries, 0u);
    EXPECT_EQ(*reloaded.settings().model.thinking_budget, 2048u);
}


// Temperature TEXT parsing must keep std::from_chars(chars_format::general)
// strictness on the portable strtod path: hex floats, NaN/Inf words,
// overflow, and trailing junk are rejected at PARSE time; otherwise-well
// formed out-of-range values are rejected later by the [0,1] check instead.

TEST(ConfigManagerUserSettings, TemperatureTextStrictDoublePortable) {
    C13Paths p("strictdouble");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto try_text = [&](const char* text) {
        return m.set_user_setting(
            "temperature",
            c13_parse(std::format("\"{}\"", text)));
    };

    // Parser rejects (naive strtod would accept the hex float and the
    // nan/inf words): must fail at the finite-number parse step.
    const char* parse_rejected[] = {
        "0x1p4", "nan", "inf", "infinity", "-inf",
        "NAN", "Inf", "1e309", "-1e309", "0.5x",
        "1.2.3", "e5", "..5", "1e", "+", "-",
    };
    for (const char* bad : parse_rejected) {
        auto out = try_text(bad);
        ASSERT_FALSE(out.has_value()) << bad;
        EXPECT_NE(out.error().message.find("finite number"),
                  std::string::npos)
            << bad << ": " << out.error().message;
    }

    // Well-formed doubles that only fail the [0,1] range prove the parser
    // accepted the spelling (error comes from the range check, not parse).
    const char* range_rejected[] = {
        "-.5", "-0.25", "1e2", "1E2",
    };
    for (const char* text : range_rejected) {
        auto out = try_text(text);
        ASSERT_FALSE(out.has_value()) << text;
        EXPECT_NE(out.error().message.find("between 0 and 1"),
                  std::string::npos)
            << text << ": " << out.error().message;
    }

    // In-range spellings fully accepted and verified by a real reload.
    for (const auto& [text, expected] :
         std::vector<std::pair<const char*, double>>{
             {"  0.5", 0.5}, {"0.5", 0.5}, {"+.5", 0.5}, {".5", 0.5},
             {"0.0", 0.0}, {"1.5e-1", 0.15}, {"1e-2", 0.01},
             {"+1", 1.0}, {"1.", 1.0}}) {
        auto out = try_text(text);
        ASSERT_TRUE(out.has_value())
            << text << ": " << (out ? "" : out.error().message);
        auto check = p.manager();
        ASSERT_TRUE(check.load().has_value());
        EXPECT_DOUBLE_EQ(*check.settings().model.temperature, expected)
            << text;
    }
}


// LOOM_MAX_TOKENS engagement is ONE range-checked predicate for both get
// provenance and set shadow disclosure: engaged iff digits parse into
// [1, uint32_max]. Returns {source_env, shadowed, effective_value}.

TEST(ConfigManagerUserSettings, MaxTokensEnvEngagementConsistent) {
    C13Paths p("envrange");
    c13_write_file(p.user_path,
                   R"JSON({"maxOutputTokens": 777})JSON");

    struct Observation {
        bool source_env;
        bool shadowed;
        std::uint32_t effective;
    };
    const auto observe = [&](const char* raw) {
        EnvironmentGuard guard("LOOM_MAX_TOKENS", raw);
        auto m = p.manager();
        EXPECT_TRUE(m.load().has_value());
        auto token = c13_parse(
            *m.agent_setting_value_json("maxOutputTokens"));
        const bool source_env =
            std::string(token.root.get("source").as_str()) == "env";
        auto out = m.set_user_setting("maxOutputTokens",
                                      c13_parse("888"));
        EXPECT_TRUE(out.has_value());
        // The post-write quiet reload re-applies the same env predicate, so
        // the effective value is read while the guard is still live.
        return Observation{
            source_env, out->shadowed, m.settings().model.max_output_tokens};
    };

    for (const char* bad : {"4294967296", "18446744073709551616", "abc",
                            "0", "-1", "12x"}) {
        auto o = observe(bad);
        EXPECT_FALSE(o.source_env) << bad;
        EXPECT_FALSE(o.shadowed) << bad;
        EXPECT_EQ(o.effective, 888u) << bad;  // file write is effective
    }
    {
        auto o = observe("12345");
        EXPECT_TRUE(o.source_env);
        EXPECT_TRUE(o.shadowed);
        // Effective value comes from the env; the 888 file write is
        // shadowed while the override is engaged.
        EXPECT_EQ(o.effective, 12345u);
    }
    // With the env unset again the last file write is effective.
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().model.max_output_tokens, 888u);
    }
}


// RFC-0001 B followup c19: a FULL project save while LOOM_MODEL /
// LOOM_MAX_TOKENS are exported re-emits the PRE-OVERLAY merged value, so the
// env value is never baked into the tracked file. Manager-level counterpart
// to the /config command tests. Each case runs under an explicit env guard
// over a hermetic C13Paths root.

TEST(ConfigManagerC19, SavePreservesFileValueUnderEnvOverlay) {
    C13Paths p("c19bake");
    c13_write_file(p.project_path, R"JSON({
  "model": "file-model-c19", "maxOutputTokens": 512
})JSON");
    // The save path's .gitignore appender walks up from CWD; run inside the
    // ancestry-clean temp root so it cannot reach a real work tree.
    CurrentPathGuard cwd_guard(p.root);

    // Case A: both env vars engaged. An unrelated full save keeps the file's
    // OWN values (never the env ones).
    {
        EnvironmentGuard model_guard("LOOM_MODEL", "env-ephemeral-c19");
        EnvironmentGuard tokens_guard("LOOM_MAX_TOKENS", "4321");
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        // Runtime reads DO see the env overlay...
        EXPECT_EQ(m.settings().model.default_model, "env-ephemeral-c19");
        EXPECT_EQ(m.settings().model.max_output_tokens, 4321u);

        m.settings_mut().display.theme = "dark";
        ASSERT_TRUE(m.save().has_value());
    }

    const std::string bytes = [&] {
        std::ifstream f(p.project_path);
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }();
    EXPECT_EQ(bytes.find("env-ephemeral-c19"), std::string::npos) << bytes;
    EXPECT_EQ(bytes.find("4321"), std::string::npos) << bytes;
    {
        auto doc = loom::utils::json::parse_file(p.project_path);
        ASSERT_TRUE(doc.has_value());
        EXPECT_EQ(doc->root().get("model").as_str(),
                  std::string_view("file-model-c19"));
        EXPECT_EQ(doc->root().get("maxOutputTokens").as_int(),
                  512);
        EXPECT_EQ(doc->root().get("theme").as_str(),
                  std::string_view("dark"));
    }

    // Case B (control): no env → a normal save persists the file's own
    // values exactly as before c19.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        EnvironmentUnsetGuard unset_tokens("LOOM_MAX_TOKENS");
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        m.settings_mut().display.show_thinking = false;
        ASSERT_TRUE(m.save().has_value());
        auto doc = loom::utils::json::parse_file(p.project_path);
        ASSERT_TRUE(doc.has_value());
        EXPECT_EQ(doc->root().get("model").as_str(),
                  std::string_view("file-model-c19"));
        EXPECT_EQ(doc->root().get("maxOutputTokens").as_int(),
                  512);
        EXPECT_EQ(doc->root().get("showThinking").as_bool(),
                  false);
    }

    // Case C: env engaged but with NO file value — the backup is the built-in
    // default, so the file gets the default (never the env value).
    C13Paths q("c19default");
    CurrentPathGuard cwd_guard_q(q.root);
    {
        EnvironmentGuard model_guard("LOOM_MODEL", "env-only-c19");
        auto m = q.manager();
        ASSERT_TRUE(m.load().has_value());
        m.settings_mut().display.theme = "light";
        ASSERT_TRUE(m.save().has_value());
    }
    {
        std::ifstream f(q.project_path);
        const std::string qbytes((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
        EXPECT_EQ(qbytes.find("env-only-c19"), std::string::npos) << qbytes;
        auto doc = loom::utils::json::parse_file(q.project_path);
        ASSERT_TRUE(doc.has_value());
        EXPECT_EQ(doc->root().get("model").as_str(),
                  std::string_view(""));
    }
}


// RFC-0001 B followup c19: an explicit set_user_setting of an env-overridden
// leaf persists the user's value, and a later unrelated save in the SAME
// instance does not revert it to the file backup. This is the exact hazard
// that ruled out a bare "prune env leaves from disk_leaves_" design: the
// post-write quiet reload re-marks env provenance, so the fix must keep the
// explicit-intent bit across it.

TEST(ConfigManagerC19, ExplicitSetSurvivesPostReloadThenUnrelatedSave) {
    C13Paths p("c19explicit");
    // The USER tier is the store set_user_setting writes and sits BELOW the
    // project file; seed the file value in the USER tier so the explicit set
    // is the effective one (matches the /config command suite, where the
    // project file is the same file the manager was constructed over).
    c13_write_file(p.user_path, R"JSON({
      "model": "seed-c19"}
    })JSON");
    CurrentPathGuard cwd_guard(p.root);

    EnvironmentGuard model_guard("LOOM_MODEL", "env-c19");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().model.default_model, "env-c19");  // env wins
        auto out = m.set_user_setting("model",
                                      c13_parse(R"("user-x-c19")"));
        ASSERT_TRUE(out.has_value());
        EXPECT_TRUE(out->shadowed);
        EXPECT_EQ(out->shadowed_by, "LOOM_MODEL");

        // An unrelated FULL save in the same instance must carry the user's X
        // (loaded from the just-patched user tier over the env overlay), not
        // the pre-write seed and not the env value.
        m.settings_mut().display.theme = "dark";
        ASSERT_TRUE(m.save().has_value());
    }
    {
        std::ifstream f(p.project_path);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("user-x-c19"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("env-c19"), std::string::npos) << bytes;
    }
    // With the env unset the user's value is what a fresh process sees.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().model.default_model, "user-x-c19");
    }
}


// RFC-0001 B followup c19: the corner the value-difference rule alone cannot
// see — the user explicitly sets a leaf to EXACTLY the env value. Without the
// explicit-intent marker the guard would re-emit the pre-overlay file value
// and silently drop the write. clear_env_provenance() is set_user_setting's
// user-intent signal and covers it.

TEST(ConfigManagerC19, ExplicitSetToEnvValueStillPersists) {
    C13Paths p("c19sameval");
    CurrentPathGuard cwd_guard(p.root);

    EnvironmentGuard model_guard("LOOM_MODEL", "same-c19");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        // The env value is what the (absent) file would lose; the user sets
        // the SAME string explicitly.
        m.clear_env_provenance("", "model");
        ASSERT_TRUE(m.save().has_value());
    }
    {
        std::ifstream f(p.project_path);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("same-c19"), std::string::npos) << bytes;
    }
}


// A leading UTF-8 BOM is stripped in both tier load and the patcher:
// a clean BOM object loads normally; BOM+object+junk is salvaged with the
// leading object's keys preserved.

TEST(ConfigManagerUserSettings, BomStrippedOnLoadAndPatch) {
    C13Paths p("bom");
    // BOM + complete leading object (carrying a sibling leaf) + trailing
    // junk. The junk keeps the §A soft-load skip; the salvage must recover
    // the leading object after stripping the BOM.
    {
        std::ofstream f(p.user_path, std::ios::binary);
        f << "\xEF\xBB\xBF"
          << R"({"maxRetries": 4, "model": "bom-kept"} TRAIL JUNK)";
    }
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());  // soft tier tolerated
        auto out = m.set_user_setting("maxRetries", c13_parse("5"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "trailing_junk_dropped");
        auto repaired = loom::utils::json::parse_file(p.user_path);
        ASSERT_TRUE(repaired.has_value());
        const auto root = repaired->root();
        EXPECT_EQ(root.get("maxRetries").as_int(), 5);
        EXPECT_EQ(root.get("model").as_str(),
                  std::string_view("bom-kept"));
    }
    {
        std::ofstream f(p.user_path, std::ios::binary);
        f << "\xEF\xBB\xBF" << R"({"maxRetries": 6})";
    }
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().network.max_retries, 6u);
        auto out = m.set_user_setting("maxRetries", c13_parse("7"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto reloaded = p.manager();
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_EQ(reloaded.settings().network.max_retries, 7u);
    }
}


// ===========================================================================
// config.json -> settings.json migration (ConfigManager::migrate_legacy_config)
// ===========================================================================


// A structured config.json is flattened into settings.json, and the old
// file is renamed to .bak.

TEST(ConfigManagerMigration, NormalMigration) {
    MigrationPaths p("normal");
    EnvironmentUnsetGuard cfg_dir_guard("LOOM_CONFIG_DIR");
    EnvironmentGuard home_guard("HOME", p.home.string());
    CurrentPathGuard cwd_guard(p.work);

    c13_write_file(p.user_config(), R"JSON({
  "model": {"default_model": "x", "max_output_tokens": 4096},
  "display": {"theme": "dark"},
  "network": {"max_retries": 3}
})JSON");

    loom::core::ConfigManager::migrate_legacy_config();

    // settings.json exists with flat camelCase keys.
    ASSERT_TRUE(fs::exists(p.user_settings()));
    auto doc = loom::utils::json::parse_file(p.user_settings());
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    ASSERT_TRUE(root.is_obj());
    // The structured model object is flattened to a flat string.
    ASSERT_TRUE(root.get("model").is_str());
    EXPECT_FALSE(root.get("model").is_obj());
    EXPECT_EQ(root.get("model").as_str(), std::string_view("x"));
    EXPECT_EQ(root.get("maxOutputTokens").as_int(), 4096);
    EXPECT_EQ(root.get("theme").as_str(), std::string_view("dark"));
    EXPECT_EQ(root.get("maxRetries").as_int(), 3);

    // The old file is gone, preserved as .bak.
    EXPECT_FALSE(fs::exists(p.user_config()));
    EXPECT_TRUE(fs::exists(p.user_config_bak()));
}


// A second migration run is a no-op: the existing settings.json is left
// untouched and the .bak from the first run is not re-renamed.

TEST(ConfigManagerMigration, MigrationIdempotent) {
    MigrationPaths p("idempotent");
    EnvironmentUnsetGuard cfg_dir_guard("LOOM_CONFIG_DIR");
    EnvironmentGuard home_guard("HOME", p.home.string());
    CurrentPathGuard cwd_guard(p.work);

    c13_write_file(p.user_config(),
                   R"JSON({"model": {"default_model": "y"}})JSON");

    loom::core::ConfigManager::migrate_legacy_config();
    ASSERT_TRUE(fs::exists(p.user_settings()));
    const auto read_settings = [&] {
        std::ifstream f(p.user_settings());
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>());
    };
    const std::string first = read_settings();

    // Second run: settings.json already exists, so the tier is skipped.
    loom::core::ConfigManager::migrate_legacy_config();
    EXPECT_EQ(read_settings(), first);
    EXPECT_TRUE(fs::exists(p.user_config_bak()));
    EXPECT_FALSE(fs::exists(p.user_config()));
}


// A corrupt config.json is skipped without crashing: no settings.json is
// created and the source is left in place (no .bak).

TEST(ConfigManagerMigration, CorruptSourceSkipped) {
    MigrationPaths p("corrupt");
    EnvironmentUnsetGuard cfg_dir_guard("LOOM_CONFIG_DIR");
    EnvironmentGuard home_guard("HOME", p.home.string());
    CurrentPathGuard cwd_guard(p.work);

    c13_write_file(p.user_config(), "THIS IS NOT JSON {{{");

    loom::core::ConfigManager::migrate_legacy_config();

    EXPECT_FALSE(fs::exists(p.user_settings()));
    EXPECT_TRUE(fs::exists(p.user_config()));
    EXPECT_FALSE(fs::exists(p.user_config_bak()));
}


// A pre-existing settings.json blocks migration of that tier: the target is
// untouched and config.json is not renamed.

TEST(ConfigManagerMigration, ExistingTargetSkipsMigration) {
    MigrationPaths p("existing");
    EnvironmentUnsetGuard cfg_dir_guard("LOOM_CONFIG_DIR");
    EnvironmentGuard home_guard("HOME", p.home.string());
    CurrentPathGuard cwd_guard(p.work);

    c13_write_file(p.user_settings(), R"JSON({"model": "keep-me"})JSON");
    c13_write_file(p.user_config(),
                   R"JSON({"model": {"default_model": "discard"}})JSON");

    loom::core::ConfigManager::migrate_legacy_config();

    auto doc = loom::utils::json::parse_file(p.user_settings());
    ASSERT_TRUE(doc.has_value());
    EXPECT_EQ(doc->root().get("model").as_str(),
              std::string_view("keep-me"));
    EXPECT_TRUE(fs::exists(p.user_config()));
    EXPECT_FALSE(fs::exists(p.user_config_bak()));
}


// Legacy structured sections still load (backward compat): every leaf is
// merged into the typed settings.

TEST(ConfigManagerUserSettings, StructuredFormatLoads) {
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    EnvironmentUnsetGuard tokens_guard("LOOM_MAX_TOKENS");
    C13Paths p("structload");
    c13_write_file(p.user_path, R"JSON({
  "model": {"default_model": "x", "temperature": 0.5},
  "display": {"theme": "dark"},
  "network": {"max_retries": 3}
})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    const auto& s = m.settings();
    EXPECT_EQ(s.model.default_model, "x");
    ASSERT_TRUE(s.model.temperature.has_value());
    EXPECT_DOUBLE_EQ(*s.model.temperature, 0.5);
    EXPECT_EQ(s.display.theme, "dark");
    EXPECT_EQ(s.network.max_retries, 3u);
}


// A save after loading a structured file re-emits flat camelCase keys: the
// structured model object is replaced outright by a flat string.

TEST(ConfigManagerUserSettings, StructuredFormatSaveConvertsToFlat) {
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    EnvironmentUnsetGuard tokens_guard("LOOM_MAX_TOKENS");
    C13Paths p("structsave");
    // Seed the PROJECT tier: save() writes to project_path_.
    c13_write_file(p.project_path, R"JSON({
  "model": {"default_model": "x", "max_output_tokens": 4096},
  "display": {"theme": "dark"},
  "network": {"max_retries": 3}
})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        // save() appends to a .gitignore found by walking up from cwd; pin
        // cwd to the git-ancestry-clean scratch root so no real work tree
        // is touched.
        CurrentPathGuard cwd_guard(p.root);
        ASSERT_TRUE(m.save().has_value());
    }
    auto doc = loom::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    // The structured model object is replaced by a flat string.
    ASSERT_TRUE(root.get("model").is_str());
    EXPECT_FALSE(root.get("model").is_obj());
    EXPECT_EQ(root.get("model").as_str(), std::string_view("x"));
    // Flat camelCase keys now exist at root.
    EXPECT_EQ(root.get("maxOutputTokens").as_int(), 4096);
    EXPECT_EQ(root.get("theme").as_str(), std::string_view("dark"));
    EXPECT_EQ(root.get("maxRetries").as_int(), 3);
}


// The verbose / vimMode flat display keys parse and round-trip through a
// save.

TEST(ConfigManagerUserSettings, VerboseAndVimModeParse) {
    C13Paths p("verbose");
    c13_write_file(p.user_path,
                   R"JSON({"verbose": true, "vimMode": true})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_TRUE(m.settings().display.verbose);
        EXPECT_TRUE(m.settings().display.vim_mode);
        CurrentPathGuard cwd_guard(p.root);
        ASSERT_TRUE(m.save().has_value());
    }
    auto doc = loom::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    EXPECT_EQ(root.get("verbose").as_bool(), true);
    EXPECT_EQ(root.get("vimMode").as_bool(), true);
}


// ===========================================================================
// RFC-0001 B followup c13d — lockfile gitignore, bounded lock, full-save
// hardening, legacy-tmp/random-name attacks.
// ===========================================================================


// Local-tier write: data file AND lock ignored, .gitignore tracked.

TEST(ConfigManagerC13d, LocalWriteIgnoresDataAndLockTracksGitignore) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("local");
    ASSERT_TRUE(repo.available);

    loom::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "stdio";
    cfg.command = "node";
    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.upsert_mcp_server(loom::core::McpStorageScope::Local, cfg)
                        .has_value());
    }
    EXPECT_TRUE(fs::exists(repo.loom / "settings.local.json"));
    EXPECT_TRUE(fs::exists(repo.loom / "settings.local.json.lock"));

    repo.git("git add -A");
    const auto status = repo.porcelain();
    EXPECT_NE(status.find(".gitignore"), std::string::npos);
    EXPECT_EQ(status.find("settings.local.json"), std::string::npos) << status;
    EXPECT_EQ(status.find("settings.local.json.lock"), std::string::npos)
        << status;
}


// Project-tier save: settings.json TRACKED, only the lock is ignored.

TEST(ConfigManagerC13d, ProjectSaveTracksDataIgnoresLock) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("project");
    ASSERT_TRUE(repo.available);

    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.save().has_value());
    }
    EXPECT_TRUE(fs::exists(repo.loom / "settings.json"));
    EXPECT_TRUE(fs::exists(repo.loom / "settings.json.lock"));

    repo.git("git add -A");
    const auto status = repo.porcelain();
    EXPECT_NE(status.find(".loom/settings.json"), std::string::npos) << status;
    EXPECT_EQ(status.find("settings.json.lock"), std::string::npos) << status;

    // The .gitignore rule names the lock, never the data file.
    std::ifstream gi(repo.root / ".gitignore");
    const std::string text((std::istreambuf_iterator<char>(gi)),
                           std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("settings.json.lock"), std::string::npos);
    EXPECT_EQ(text.find("\nsettings.json\n"), std::string::npos);
}


// Repeated local/project writes never duplicate ignore lines.

TEST(ConfigManagerC13d, GitignoreLinesAreIdempotent) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("idem");
    ASSERT_TRUE(repo.available);

    loom::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "stdio";
    cfg.command = "node";
    auto m = repo.manager();
    ASSERT_TRUE(m.load().has_value());
    for (int i = 0; i < 3; ++i) {
        cfg.name = std::string("srv") + std::to_string(i);
        ASSERT_TRUE(m.upsert_mcp_server(loom::core::McpStorageScope::Local, cfg)
                        .has_value());
    }
    ASSERT_TRUE(m.save().has_value());
    ASSERT_TRUE(m.save().has_value());

    const std::string text = [] {
        std::ifstream f(fs::current_path() / ".gitignore");
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }();
    EXPECT_EQ(c6_count_occurrences(text, "settings.local.json\n"), 1u) << text;
    EXPECT_EQ(c6_count_occurrences(text, "settings.local.json.lock\n"), 1u)
        << text;
    EXPECT_EQ(c6_count_occurrences(text, "settings.json.lock\n"), 1u) << text;
}


// User tier writes create no .gitignore at all.

TEST(ConfigManagerC13d, UserWritesNoGitignore) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("user");
    ASSERT_TRUE(repo.available);

    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.set_user_setting("maxRetries", c13_parse("2"))
                        .has_value());
    }
    EXPECT_FALSE(fs::exists(repo.root / ".gitignore"));
    // The user-tier write lives outside the project tree; .loom stays
    // empty (it is a pre-created empty directory from the fixture).
    EXPECT_EQ(std::distance(fs::directory_iterator(repo.loom),
                            fs::directory_iterator()), 0);
    EXPECT_TRUE(fs::exists(repo.outside / "user.json"));
}


// A 2s external lock holder makes the writer WAIT and then succeed; a
// long holder trips the ~10s WALL-CLOCK bound with a clean, specific
// error and leaves the file and directory untouched. Holder readiness is
// a pipe handshake — the child signals only AFTER its flock succeeds — so
// no fixed sleep guesses child readiness. That guess (plus an
// attempt-counted "bound") made the old version acquire at ~13s under a
// loaded 3-vCPU mac runner instead of timing out at 10s.

TEST(ConfigManagerC13d, BoundedLockWaitSuccessThenTimeout) {
    C13Paths p("lockwait");
    const auto lock_path = p.user_path.string() + ".lock";

    struct Holder {
        pid_t pid;
        int read_fd;
    };
    // Kills+reaps a still-held holder on scope exit, so an early ASSERT_*
    // failure (including a failed readiness handshake) can never leak a
    // forked child holding the lock. The normal path reaps explicitly and
    // disarms the guard with release().
    struct HolderReaper {
        pid_t pid = -1;
        HolderReaper() = default;
        explicit HolderReaper(pid_t child) : pid(child) {}
        HolderReaper(const HolderReaper&) = delete;
        HolderReaper& operator=(const HolderReaper&) = delete;
        ~HolderReaper() {
            if (pid > 0) {
                (void)::kill(pid, SIGKILL);
                int status = 0;
                (void)::waitpid(pid, &status, 0);
            }
        }
        void release() { pid = -1; }
    };
    // Fork a child that takes LOCK_EX on lock_path, then writes one byte
    // to the pipe only once the flock is actually held ('R'; 'E' if the
    // open/flock failed), so the parent can block on read() instead of
    // guessing readiness with a sleep. Plain ::pipe() — pipe2() is
    // Linux-only and this test must stay macOS-portable.
    auto spawn_holder = [&](int seconds) -> Holder {
        int pipefd[2];
        if (::pipe(pipefd) != 0) {
            ADD_FAILURE() << "pipe: " << std::strerror(errno);
            return {-1, -1};
        }
        const pid_t pid = ::fork();
        if (pid < 0) {
            ADD_FAILURE() << "fork: " << std::strerror(errno);
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            return {-1, -1};
        }
        if (pid == 0) {
            ::close(pipefd[0]);
            const int fd = ::open(lock_path.c_str(),
                                  O_RDWR | O_CREAT | O_CLOEXEC, 0600);
            const char fail = 'E';
            if (fd < 0) {
                (void)::write(pipefd[1], &fail, 1);
                _exit(10);
            }
            if (::flock(fd, LOCK_EX) != 0) {
                (void)::write(pipefd[1], &fail, 1);
                _exit(11);
            }
            const char ready = 'R';
            if (::write(pipefd[1], &ready, 1) != 1) _exit(12);
            for (int i = 0; i < seconds * 10; ++i) {
                struct timespec ts{0, 100 * 1000 * 1000};
                ::nanosleep(&ts, nullptr);
            }
            _exit(0);
        }
        ::close(pipefd[1]);  // parent's only handle is the read end
        return {pid, pipefd[0]};
    };
    // Block until the holder child has reported its flock. ASSERT (not
    // EXPECT): an 'E'/short read aborts the phase, and the armed
    // HolderReaper reaps the child on the way out.
    auto await_ready = [&](int fd) {
        char b = 0;
        ssize_t n = 0;
        do {
            n = ::read(fd, &b, 1);
        } while (n == -1 && errno == EINTR);
        ASSERT_EQ(n, 1);
        ASSERT_EQ(b, 'R') << "holder child failed to acquire the lock";
        ::close(fd);
    };

    // Short holder: wait then succeed.
    {
        Holder h = spawn_holder(2);
        ASSERT_GT(h.pid, 0);
        HolderReaper reaper(h.pid);
        await_ready(h.read_fd);
        auto m = p.manager();
        const auto start = std::chrono::steady_clock::now();
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        ASSERT_TRUE(out.has_value()) << (out ? "" : out.error().message);
        EXPECT_GE(elapsed, 1000) << "writer should have waited on the lock";
        int status = 0;
        ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
        reaper.release();  // holder exited and is reaped
        auto fresh = p.manager();
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_EQ(fresh.settings().network.max_retries, 1u);
    }

    // Long holder (30s nominal — well past the 10s bound even with
    // scheduler-stalled sleeps): bounded wait fails closed with the
    // specific message.
    {
        Holder h = spawn_holder(30);
        ASSERT_GT(h.pid, 0);
        HolderReaper reaper(h.pid);
        await_ready(h.read_fd);
        c13_write_file(p.user_path, R"JSON({"maxRetries": 9})JSON");
        auto m = p.manager();
        const auto start = std::chrono::steady_clock::now();
        auto out = m.set_user_setting("maxRetries", c13_parse("7"));
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("another Loom process is updating"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find(lock_path), std::string::npos);
        EXPECT_GE(elapsed, 9000) << "should wait up to the ~10s bound";
        EXPECT_LE(elapsed, 14000) << "deadline must hold under scheduler load";
        ::kill(h.pid, SIGKILL);
        int status = 0;
        ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
        reaper.release();  // holder killed and reaped

        // File unmodified; no tmp debris of any kind.
        auto doc = loom::utils::json::parse_file(p.user_path);
        ASSERT_TRUE(doc.has_value());
        EXPECT_EQ(doc->root().get("maxRetries").as_int(), 9);
        for (const auto& entry : fs::directory_iterator(p.root)) {
            const auto name = entry.path().filename().string();
            EXPECT_EQ(name.find(".tmp."), std::string::npos) << name;
            EXPECT_NE(name, "user.json.tmp") << name;
        }
    }
}


// Same FIFO-at-lock-name defense for ConfigFileLock: set_user_setting must
// surface ConfigWriteError with the existing open-failure shape (not the
// symlink-specific wording), leave the FIFO in place, create no user.json,
// and return without awaiting the 10s bound.

TEST(ConfigManagerC15, FifoLockNameRejectedWithoutBlocking) {
    C13Paths p("fifolock");
    const std::string lock_path = p.user_path.string() + ".lock";
    ASSERT_EQ(::mkfifo(lock_path.c_str(), 0600), 0);

    auto m = p.manager();
    const auto start = std::chrono::steady_clock::now();
    auto out = m.set_user_setting("maxRetries", c13_parse("1"));
    const auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    ASSERT_FALSE(out.has_value());
    EXPECT_NE(out.error().message.find("Cannot open config file for writing"),
              std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("user.json.lock"),
              std::string::npos)
        << out.error().message;
    EXPECT_EQ(out.error().message.find("symbolic link"), std::string::npos)
        << "FIFO rejection must not borrow the symlink-specific wording";
    EXPECT_LT(elapsed, 3000)
        << "non-regular lock name must fail fast, never await the deadline";
    EXPECT_FALSE(fs::exists(p.user_path))
        << "failed lock acquisition must bail before any data-file write";
    std::error_code ec;
    EXPECT_TRUE(fs::is_fifo(lock_path, ec))
        << "the FIFO itself must never be unlinked or replaced";
}


// Stale regular-file/FIFO debris at the reserved legacy tmp name is an old
// crash artifact: auto-unlinked, the write proceeds cleanly.

TEST(ConfigManagerC13d, LegacyTmpRegularAndFifoDebrisAutoRemoved) {
    C13Paths p("legacydebris");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    {
        std::ofstream(p.user_path.string() + ".tmp") << "old crash junk";
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(p.user_path.string() + ".tmp"));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 1);
    }
    {
        ASSERT_EQ(::mkfifo((p.user_path.string() + ".tmp").c_str(), 0600), 0);
        auto out = m.set_user_setting("maxRetries", c13_parse("2"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(p.user_path.string() + ".tmp"));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("maxRetries").as_int(), 2);
    }
}


// A symlinked config LEAF fails closed with the specific message; a
// symlinked lock path names the lock rather than its target.

TEST(ConfigManagerC13d, SymlinkedLeafAndLockRefusedWithSpecificErrors) {
    C13Paths p("leafsym");
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-leaf-5521";
    { std::ofstream(victim) << kCanary; }

    // Symlinked data leaf.
    fs::create_symlink(victim, p.user_path);
    {
        auto m = p.manager();
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("symbolic link"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find("symlinked configuration file"),
                  std::string::npos);
    }
    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
        std::error_code ec;
        EXPECT_TRUE(fs::is_symlink(p.user_path, ec));
    }

    // Symlinked lock path.
    fs::remove(p.user_path);
    fs::create_symlink(victim, p.user_path.string() + ".lock");
    {
        auto m = p.manager();
        auto out = m.set_user_setting("maxRetries", c13_parse("1"));
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("symlinked configuration lock"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find("user.json.lock"),
                  std::string::npos);
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
}


// save() through a pre-placed tmp symlink: victim intact, regular JSON.

TEST(ConfigManagerC13d, FullSaveTmpSymlinkNeverFollowed) {
    C13Paths p("savesym");
    CurrentPathGuard cwd_guard(p.root);
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-save-8830";
    { std::ofstream(victim) << kCanary; }
    fs::create_symlink(victim, p.project_path.string() + ".tmp");

    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.save().has_value());

    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.project_path, ec).type(),
              fs::file_type::regular);
    auto doc = loom::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    EXPECT_TRUE(doc->root().is_obj());
    EXPECT_FALSE(fs::exists(p.project_path.string() + ".tmp"));
}


// save() preserves a pre-existing 0600 mode across the replace.

TEST(ConfigManagerC13d, FullSavePreservesMode) {
    if (::getuid() == 0) GTEST_SKIP() << "modes are bypassed for root";
    C13Paths p("savemode");
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.project_path, R"JSON({"maxRetries": 1})JSON");
    fs::permissions(p.project_path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.save().has_value());
    const auto mode = fs::status(p.project_path).permissions() & fs::perms::mask;
    EXPECT_EQ(mode, fs::perms::owner_read | fs::perms::owner_write);
}


// save() through a symlinked leaf refuses with the specific error.

TEST(ConfigManagerC13d, FullSaveSymlinkedLeafRefused) {
    C13Paths p("saveleaf");
    CurrentPathGuard cwd_guard(p.root);
    const auto victim = p.root / "victim.txt";
    { std::ofstream(victim) << "VICTIM-c13d-saveleaf"; }
    fs::create_symlink(victim, p.project_path);
    auto m = p.manager();
    // No load(): a symlinked project leaf already hard-fails the tier read;
    // here we verify save() itself fails closed when reached directly.
    auto out = m.save();
    ASSERT_FALSE(out.has_value());
    EXPECT_NE(out.error().message.find("symlinked configuration file"),
              std::string::npos);
}


// One full save() racing ten patched setters: every child succeeds and
// the final file (and no intermediate one observed) is parseable JSON with
// no tmp debris — never torn, last-writer-wins by nature.

TEST(ConfigManagerC13d, ConcurrentSaveAndPatchersAlwaysParseable) {
    C13Paths p("saveconcurrent");
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.user_path, R"JSON({"maxRetries": 1})JSON");

    constexpr int kChildren = 11;
    std::array<int, kChildren> pids{};
    for (int i = 0; i < kChildren; ++i) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = p.manager();
            (void)m.load();
            if (i == kChildren - 1) {
                _exit(m.save()
                          ? 0 : 2);
            }
            const char* keys[] = {"maxRetries",
                                  "maxOutputTokens",
                                  "contextWindowSize",
                                  "thinkingBudget",
                                  "extendedThinking"};
            const char* vals[] = {"100", "20000", "300000", "4096", "true"};
            const int k = i % 5;
            auto out = m.set_user_setting(
                keys[k], c13_parse(vals[k]));
            _exit(out.has_value() ? 0 : 2);
        }
        pids[static_cast<std::size_t>(i)] = pid;
    }
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }

    // User file: every patcher landed distinct keys (project save targets
    // a different path, so it cannot clobber the user file).
    auto doc = loom::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    ASSERT_TRUE(doc->root().is_obj());
    EXPECT_EQ(doc->root().get("maxOutputTokens").as_int(),
              20000);
    EXPECT_EQ(doc->root().get("extendedThinking").as_bool(),
              true);
    // Project save produced a parseable full document.
    auto project_doc = loom::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(project_doc.has_value());
    EXPECT_TRUE(project_doc->root().is_obj());
    for (const auto& entry : fs::directory_iterator(p.root)) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp."),
                  std::string::npos);
    }
}


// Pre-spraying symlinks at predictable legacy-style tmp names cannot block
// the random-suffixed writer or reach the victim.

TEST(ConfigManagerC13d, PresprayedPredictableTmpNamesDefeated) {
    C13Paths p("prespray");
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-prespray-2299";
    { std::ofstream(victim) << kCanary; }
    const pid_t pid = ::getpid();
    for (int i = 0; i < 64; ++i) {
        fs::create_symlink(
            victim,
            p.user_path.string() + ".tmp." + std::to_string(pid) + "." +
                std::to_string(i));
    }
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("maxRetries", c13_parse("3"));
    ASSERT_TRUE(out.has_value()) << out.error().message;

    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.user_path, ec).type(),
              fs::file_type::regular);
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    EXPECT_EQ(root.root.get("maxRetries").as_int(), 3);
}


// ===========================================================================
// RFC-0001 B followup c13e — git-worktree-gated ignores + same-path
// save/patch contention.
// ===========================================================================

// Plain non-git directory: local AND project writes must NOT create any
// .gitignore anywhere (the data write itself is unaffected).

TEST(ConfigManagerC13e, PlainDirectoryWritesNoGitignore) {
    // Anchored under the shared git-ancestry-clean temp base (this box
    // carries a stray /tmp/.git, so XDG_RUNTIME_DIR / /dev/shm win).
    const auto base = c13_clean_temp_base();
    if (!base) GTEST_SKIP() << "no .git-free temp base directory available";

    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = *base / ("loom_c13e_plain_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // Snapshot any PRE-EXISTING (unrelated) .gitignore on the chain so the
    // post-write assertion is immune to a dirty shared temp base.
    const auto ignores_before = c13_gitignores_on_chain(root, *base);

    loom::core::McpServerConfig local;
    local.name = "ls";
    local.transport = "stdio";
    local.command = "node";
    loom::core::McpServerConfig project = local;
    project.name = "ps";

    {
        loom::core::ConfigManager m(user_path, project_path, local_path);
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.upsert_mcp_server(loom::core::McpStorageScope::Local, local)
                        .has_value());
        ASSERT_TRUE(m.upsert_mcp_server(loom::core::McpStorageScope::Project,
                                        project).has_value());
        ASSERT_TRUE(m.save().has_value());
    }
    EXPECT_TRUE(fs::exists(local_path));
    EXPECT_TRUE(fs::exists(project_path));
    // No NEW .gitignore appeared anywhere on the walk-up chain to the base.
    const auto ignores_after = c13_gitignores_on_chain(root, *base);
    for (const auto& path : ignores_after) {
        EXPECT_TRUE(ignores_before.count(path) != 0)
            << "unexpected new .gitignore: " << path;
    }
    EXPECT_FALSE(fs::exists(root / ".gitignore"));
    // Data files are still correct.
    auto ldoc = loom::utils::json::parse_file(local_path);
    ASSERT_TRUE(ldoc.has_value());
    EXPECT_TRUE(ldoc->root().get("mcpServers").has("ls"));

    std::error_code ec;
    fs::remove_all(root, ec);
}


// Nested cwd inside a git repo: the ignore lines land in the REPO ROOT
// .gitignore via walk-up, not the nested working directory.

TEST(ConfigManagerC13e, NestedCwdAppendsAtRepoRoot) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto base = fs::temp_directory_path() /
                      ("loom_c13e_nested_" + std::to_string(suffix));
    fs::create_directories(base);
    const auto repo = base / "r";
    fs::create_directories(repo);
    {
        CurrentPathGuard init_guard(repo);
        ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);
    }
    const auto nested = repo / "sub" / "deep";
    fs::create_directories(nested);

    const auto user_path    = nested / "user.json";
    const auto project_path = nested / ".loom" / "settings.json";
    const auto local_path   = nested / ".loom" / "settings.local.json";

    {
        CurrentPathGuard cwd_guard(nested);
        loom::core::ConfigManager m(user_path, project_path, local_path);
        ASSERT_TRUE(m.load().has_value());
        loom::core::McpServerConfig cfg;
        cfg.name = "deep-srv";
        cfg.transport = "stdio";
        cfg.command = "node";
        ASSERT_TRUE(m.upsert_mcp_server(loom::core::McpStorageScope::Local, cfg)
                        .has_value());
        ASSERT_TRUE(m.save().has_value());

        // Nothing written at the nested cwd level.
        EXPECT_FALSE(fs::exists(nested / ".gitignore"));
        // Lines landed at the walk-up repo root.
        const auto root_gi = repo / ".gitignore";
        ASSERT_TRUE(fs::exists(root_gi));
        std::ifstream f(root_gi);
        const std::string text((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
        EXPECT_NE(text.find("settings.local.json.lock"), std::string::npos);
        EXPECT_NE(text.find("settings.local.json\n"), std::string::npos);
        EXPECT_NE(text.find("settings.json.lock"), std::string::npos);
        EXPECT_EQ(text.find("settings.json\n"), std::string::npos);
    }
    fs::remove_all(base);
}


// Full save() and patched set_user_setting writes hit the SAME user file
// concurrently: every child exits 0 (bounded lock/CAS keep failures clean),
// and a sampled sweep plus the final file always parse as JSON objects with
// no tmp debris. save() can only target project_path_, so the manager is
// constructed with project_path_ aliasing the user config location.

TEST(ConfigManagerC13e, SaveAndPatchesContendOnSameFile) {
    // C13Paths anchors p.root under a git-ANCESTRY-CLEAN base (this box
    // carries a stray /tmp/.git, which would otherwise make forked savers
    // write settings.json.lock into /tmp/.gitignore), so the claim below is
    // now true rather than merely CI-lucky.
    C13Paths p("samefile");
    // Forked children inherit cwd: keep every ignore-append probe inside
    // the clean temp root, whose walk-up contains no .git marker, so no
    // .gitignore can be created anywhere up the chain.
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.project_path, R"JSON({
  "maxRetries": 1,
  "model": "seed-model"
})JSON");

    // Snapshot pre-existing .gitignore files on the exact chain this
    // fixture uses (recorded in p.base) before the fork storm.
    const auto ignores_before =
        c13_gitignores_on_chain(p.root, p.base);

    // Manager whose SAVE target is the same file the patchers write:
    // save() writes project_path_; set_user_setting writes
    // user_path_ — point both at one path.
    auto make_aligned = [&] {
        return loom::core::ConfigManager(p.project_path, p.project_path,
                                       p.project_path.string() + ".local");
    };

    constexpr int kPatchers = 8;
    constexpr int kSavers = 4;
    constexpr int kTotal = kPatchers + kSavers;
    std::array<pid_t, kTotal> pids{};
    int idx = 0;
    for (int s = 0; s < kSavers; ++s) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = make_aligned();
            (void)m.load();
            // Mutate so each full-save document is distinct, then replace
            // the SAME file the patchers update.
            m.settings_mut().network.max_retries =
                static_cast<std::uint32_t>(1000 + s);
            _exit(m.save() ? 0 : 2);
        }
        pids[static_cast<std::size_t>(idx++)] = pid;
    }
    const std::array<std::string_view, 5> keys = {{
        "maxRetries",
        "maxOutputTokens",
        "contextWindowSize",
        "extendedThinking",
        "model",
    }};
    for (int k = 0; k < kPatchers; ++k) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = make_aligned();
            (void)m.load();
            const auto& key = keys[static_cast<std::size_t>(k % keys.size())];
            const char* payload =
                key == "model" ? R"("contend-model")"
                : key == "extendedThinking" ? "true"
                : key == "maxOutputTokens" ? "2048"
                : key == "contextWindowSize" ? "100000"
                : "7";
            auto out = m.set_user_setting(key, c13_parse(payload));
            _exit(out.has_value() ? 0 : 2);
        }
        pids[static_cast<std::size_t>(idx++)] = pid;
    }
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        // Only 0 (success) or a CLEAN lock/CAS-style error (2) are allowed;
        // crash/signal would fail here. With the bounded flock + CAS retry
        // the writer converges, so expect all-success in practice.
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }

    // Final file is a parseable object.
    auto doc = loom::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    EXPECT_TRUE(doc->root().is_obj());
    // No torn tmp leftovers; no stray local file (that path is never
    // written here — local_path_ aliases a different name).
    for (const auto& entry : fs::directory_iterator(p.root)) {
        const auto name = entry.path().filename().string();
        EXPECT_EQ(name.find(".tmp"), std::string::npos) << name;
    }
    // Snapshot-negative assertion: the fork storm created NO new .gitignore
    // anywhere on the exact chain from the temp root to p's recorded base
    // (pre-existing unrelated ignores in a shared base stay allowed).
    const auto ignores_after =
        c13_gitignores_on_chain(p.root, p.base);
    for (const auto& path : ignores_after) {
        EXPECT_TRUE(ignores_before.count(path) != 0)
            << "unexpected new .gitignore: " << path;
    }
}

// End of file
