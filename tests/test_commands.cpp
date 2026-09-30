/// @file test_commands.cpp
/// @brief Command system smoke tests aligned with current C++ module APIs.

#include <gtest/gtest.h>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

import std;
import cc.commands.command;
import cc.commands.registry;
import cc.query.query_engine;
import cc.tools.tool;
import cc.types.types;
import cc.commands.agents;
import cc.commands.clear;
import cc.commands.config;
import cc.config.config;
import cc.commands.help;
import cc.commands.hooks;
import cc.commands.insights;
import cc.commands.model;
import cc.commands.mcp_cmd;
import cc.commands.rewind;
import cc.commands.plugin_cmd;
import cc.commands.plugin_ui_data;
import cc.commands.plugin_parse_args;
import cc.utils.error;
import cc.utils.json;
import cc.commands.terminal_setup;
import cc.platform.hyperlink;
import cc.services.mcp.xaa_idp_login;

namespace {

namespace fs = std::filesystem;

/// Git-ancestry-clean temp root (mirrors the helper in test_services):
/// this box carries a stray /tmp/.git, so a chdir under a plain /tmp tree
/// would let the config gitignore appender reach it.
[[nodiscard]] bool cmd_ancestry_clean(const fs::path& base) {
    std::error_code ec;
    for (auto d = base; ; d = d.parent_path()) {
        if (fs::exists(d / ".git", ec)) return false;
        if (d.parent_path() == d || d.parent_path().empty()) return true;
    }
}

[[nodiscard]] std::optional<fs::path> cmd_clean_temp_base() {
    std::error_code ec;
    for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
        if (const char* v = std::getenv(var);
            v != nullptr && fs::is_directory(v, ec) && !ec &&
            cmd_ancestry_clean(v)) {
            return fs::path(v);
        }
        ec.clear();
    }
    if (fs::is_directory("/dev/shm", ec) && !ec &&
        cmd_ancestry_clean("/dev/shm")) {
        return fs::path("/dev/shm");
    }
    if (cmd_ancestry_clean(fs::temp_directory_path())) {
        return fs::temp_directory_path();
    }
    return std::nullopt;
}

[[nodiscard]] fs::path cmd_make_temp_root(std::string_view name) {
    static std::atomic<unsigned> counter{0};
    const fs::path base_dir =
        cmd_clean_temp_base().value_or(fs::temp_directory_path());
    for (int attempt = 0; attempt < 64; ++attempt) {
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
        if (fs::is_directory(root, probe_ec) && !probe_ec) return root;
    }
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

cc::core::CommandContext ctx(std::vector<std::string> args = {}, std::string raw = {}) {
    return cc::core::CommandContext{
        .args = std::move(args),
        .raw_input = std::move(raw),
        .cwd = {},
    };
}

/// Read a file's bytes (empty string when absent).
[[nodiscard]] std::string cmd_read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

std::vector<cc::core::Message> compact_runtime_messages(void* state) {
    auto* engine = static_cast<cc::core::QueryEngine*>(state);
    return engine ? engine->get_conversation() : std::vector<cc::core::Message>{};
}

cc::core::VoidResult compact_runtime_apply(void* state) {
    auto* engine = static_cast<cc::core::QueryEngine*>(state);
    if (!engine) {
        return std::unexpected(cc::core::Error::make(
            cc::core::ErrorCode::InternalError,
            "No active query engine is available for compaction"));
    }
    auto compacted = engine->compact_conversation();
    if (!compacted) {
        return std::unexpected(cc::core::Error::make(
            cc::core::ErrorCode::InternalError,
            compacted.error().format()));
    }
    return cc::core::VoidResult{};
}

} // namespace

TEST(CommandRegistry, ParsesSlashCommandsAndArguments) {
    auto parsed = cc::core::CommandRegistry::parse("/config get model.default_model");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->name, "config");
    ASSERT_EQ(parsed->args.size(), 2u);
    EXPECT_EQ(parsed->args[0], "get");
    EXPECT_EQ(parsed->args[1], "model.default_model");
    EXPECT_EQ(parsed->raw, "/config get model.default_model");

    EXPECT_FALSE(cc::core::CommandRegistry::parse("not a command").has_value());
}

TEST(CommandRegistry, ExecutesLegacyCommandsAndAliases) {
    cc::core::CommandRegistry registry;
    registry.register_command(cc::core::CommandRegistration{
        .name = "echo",
        .description = "Echo input",
        .usage = "/echo <text>",
        .handler = [](const cc::core::CommandContext& command_ctx) {
            return cc::core::CommandResult::success(command_ctx.args.empty() ? "" : command_ctx.args.front());
        },
        .aliases = {"say"},
        .hidden = false,
    });

    EXPECT_TRUE(registry.contains("echo"));
    EXPECT_TRUE(registry.contains("say"));

    auto result = registry.execute("/say hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ok);
    EXPECT_EQ(result->message, "hello");

    auto missing = registry.execute("/missing");
    ASSERT_TRUE(missing.has_value());
    EXPECT_FALSE(missing->ok);
}

TEST(CommandRegistry, RegistersTypedCommandsAndCompletesNames) {
    cc::core::CommandRegistry registry;
    registry.register_command<cc::commands::HelpCommand>();
    registry.register_command<cc::commands::ClearCommand>();

    EXPECT_EQ(registry.size(), 2u);
    EXPECT_NE(registry.get("help"), nullptr);
    EXPECT_NE(registry.get("h"), nullptr);

    auto completions = registry.complete("/he");
    ASSERT_FALSE(completions.empty());
    EXPECT_EQ(completions.front(), "/help");

    auto executed = registry.execute("/h --shortcuts");
    ASSERT_TRUE(executed.has_value());
    EXPECT_TRUE(executed->ok);
    EXPECT_NE(executed->message.find("Ctrl+C"), std::string::npos);
}

TEST(AppCommandRegistry, ReportsCommandPermissionLevels) {
    EXPECT_EQ(cc::commands::command_permission("help"), cc::commands::CommandPermission::ReadOnly);
    EXPECT_EQ(cc::commands::command_permission("clear"), cc::commands::CommandPermission::ReadWrite);
    EXPECT_EQ(cc::commands::command_permission("unknown"), cc::commands::CommandPermission::None);
}

TEST(AppCommandRegistry, DispatchesMigratedRuntimeCommands) {
    cc::commands::AppCommandRegistry registry;

    EXPECT_GT(registry.command_count(), 0u);
    EXPECT_TRUE(registry.has_command("commit"));
    EXPECT_TRUE(registry.has_command("mcp"));
    EXPECT_TRUE(registry.has_command("ant-trace"));
    EXPECT_TRUE(registry.has_command("version"));
    EXPECT_TRUE(registry.has_command("exit"));

    auto help = registry.execute("/help", ctx());
    ASSERT_TRUE(help.has_value());
    EXPECT_TRUE(help->ok);
    ASSERT_TRUE(help->metadata.has_value());
    EXPECT_EQ(*help->metadata, "UI:help");

    auto mcp = registry.execute("/mcp list", ctx());
    ASSERT_TRUE(mcp.has_value());
    EXPECT_TRUE(mcp->ok);
    EXPECT_EQ(mcp->message.find("Unknown command"), std::string::npos);

    auto ant_trace = registry.execute("/ant-trace request-42", ctx());
    ASSERT_TRUE(ant_trace.has_value());
    EXPECT_TRUE(ant_trace->ok);
    EXPECT_NE(ant_trace->message.find("ANT trace snapshot"), std::string::npos);
    EXPECT_EQ(ant_trace->message.find("No dedicated local action"), std::string::npos);

    auto version = registry.execute("/version detail", ctx());
    ASSERT_TRUE(version.has_value());
    EXPECT_TRUE(version->ok);
    EXPECT_NE(version->message.find("loom 1.0.0-cpp"), std::string::npos);
    EXPECT_EQ(version->message.find("No dedicated local action"), std::string::npos);

    auto exit = registry.execute("/exit", ctx());
    ASSERT_TRUE(exit.has_value());
    EXPECT_EQ(exit->metadata, "EXIT");
}

// RFC-0001 B followup c6 review: the /mcp add flag loop used to double-
// consume every value-flag (the inner consumer AND the loop tail both
// advanced), so the flag's VALUE was revisited as a positional token:
// `--scope project` leaked a phantom "project" stdio arg and remote
// `--url`/`-H` values hard-failed as unexpected arguments. Driven through
// the real AppCommandRegistry with temp HOME / LOOM_CONFIG_DIR / CWD.
TEST(AppCommandRegistry, McpAddFlagValuesConsumedExactlyOnce) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c6_flags_");
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work);

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    {
        // Construct AFTER env/cwd are set: the command's default
        // ConfigManager binds its paths in its constructor.
        cc::commands::AppCommandRegistry registry;

        // stdio scoped add: the scope value must not become a phantom arg.
        auto scoped = registry.execute(
            "/mcp add s1 node serve.js --scope project", ctx());
        ASSERT_TRUE(scoped.has_value());
        ASSERT_TRUE(scoped->ok) << scoped->message;

        auto project = cc::utils::json::parse_file(work / ".loom" / "config.json");
        ASSERT_TRUE(project.has_value());
        const auto s1 = project->root().get("mcpServers").get("s1");
        ASSERT_TRUE(s1.is_obj());
        EXPECT_EQ(s1.get("command").as_str(), std::string_view("node"));
        ASSERT_TRUE(s1.get("args").is_arr());
        ASSERT_EQ(s1.get("args").size(), 1u);
        EXPECT_EQ(s1.get("args").at(0).as_str(), std::string_view("serve.js"));
        EXPECT_EQ(s1.get("configScope").as_str(), std::string_view("project"));

        // Remote add with a positional URL + -H header + --url/value flag:
        // before the fix every flag value was revisited and this hard-failed
        // ("Unexpected argument for remote MCP server").
        auto remote = registry.execute(
            "/mcp add r1 https://mcp.example.com/mcp -H X-Test:1 --url https://mcp.example.com/v2",
            ctx());
        ASSERT_TRUE(remote.has_value());
        ASSERT_TRUE(remote->ok) << remote->message;

        // Default scope is local: header must land in the local tier file.
        auto local = cc::utils::json::parse_file(work / ".loom" / "config.local.json");
        ASSERT_TRUE(local.has_value());
        const auto r1 = local->root().get("mcpServers").get("r1");
        ASSERT_TRUE(r1.is_obj());
        EXPECT_EQ(r1.get("type").as_str(), std::string_view("http"));
        EXPECT_EQ(r1.get("url").as_str(), std::string_view("https://mcp.example.com/v2"));
        EXPECT_EQ(r1.get("headers").get("X-Test").as_str(), std::string_view("1"));
        EXPECT_FALSE(r1.has("args"));
        EXPECT_FALSE(r1.has("command"));

        // Two --scope flags: LAST token wins everywhere.
        auto two_scopes = registry.execute(
            "/mcp add s2 node s2.js --scope project --scope user", ctx());
        ASSERT_TRUE(two_scopes.has_value());
        ASSERT_TRUE(two_scopes->ok) << two_scopes->message;

        auto user = cc::utils::json::parse_file(cfg / "config.json");
        ASSERT_TRUE(user.has_value());
        EXPECT_TRUE(user->root().get("mcpServers").has("s2"));
        EXPECT_FALSE(project->root().get("mcpServers").has("s2"));
    }

    cleanup();
}

// RFC-0001 B followup c8: ConfigCommand held a default-constructed
// ConfigManager and never called load(), so /config list and /config get
// rendered built-in DEFAULTS instead of the user's files. Seeds the project
// .loom/config.json and drives the real AppCommandRegistry (see c6 above).
TEST(AppCommandRegistry, ConfigListGetReflectLoadedConfig) {
    namespace fs = std::filesystem;
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_cmd_c8_list_" + std::to_string(suffix));
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    // load() applies LOOM_MODEL over file config; unset it so the seeded
    // model value is what comes back.
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    {
        {
            std::ofstream seed(work / ".loom" / "config.json");
            seed << "{\n"
                    "  \"model\": { \"default_model\": \"custom-model-x\" },\n"
                    "  \"display\": { \"theme\": \"light\" },\n"
                    "  \"network\": { \"timeout_seconds\": 42 }\n"
                    "}\n";
        }

        // Construct AFTER env/cwd are set: the command's default
        // ConfigManager binds its paths in its constructor.
        cc::commands::AppCommandRegistry registry;

        auto list = registry.execute("/config list", ctx());
        ASSERT_TRUE(list.has_value());
        ASSERT_TRUE(list->ok) << list->message;
        // Seeded values appear...
        EXPECT_NE(list->message.find("custom-model-x"), std::string::npos);
        EXPECT_NE(list->message.find("= light"), std::string::npos);
        EXPECT_NE(list->message.find("= 42s"), std::string::npos);
        // ...and the defaults they replaced do not.
        EXPECT_EQ(list->message.find("claude-sonnet-4-20250514"), std::string::npos);
        EXPECT_EQ(list->message.find("= auto"), std::string::npos);
        EXPECT_EQ(list->message.find("= 120s"), std::string::npos);

        auto get_model = registry.execute("/config get model.default_model", ctx());
        ASSERT_TRUE(get_model.has_value());
        ASSERT_TRUE(get_model->ok) << get_model->message;
        EXPECT_EQ(get_model->message, "model.default_model = custom-model-x");

        auto get_theme = registry.execute("/config get display.theme", ctx());
        ASSERT_TRUE(get_theme.has_value());
        ASSERT_TRUE(get_theme->ok) << get_theme->message;
        EXPECT_EQ(get_theme->message, "display.theme = light");

        auto get_timeout = registry.execute("/config get network.timeout", ctx());
        ASSERT_TRUE(get_timeout.has_value());
        ASSERT_TRUE(get_timeout->ok) << get_timeout->message;
        EXPECT_EQ(get_timeout->message, "network.timeout = 42");
    }

    cleanup();
}

// RFC-0001 B followup c8: /config set applied onto default settings and the
// full save rewrote the project file, destroying its model/display values
// and project-owned mcpServers, while copying user-scope-only MCP entries
// (with Authorization headers) into the tracked file. After the fix the set
// mutates the LOADED settings and C6's §B filter still holds at the save.
TEST(AppCommandRegistry, ConfigSetPreservesExistingSections) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c8_preserve_");
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    auto read_file = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    };

    const fs::path project_path = work / ".loom" / "config.json";
    const fs::path user_path = cfg / "config.json";

    {
        {
            std::ofstream seed(project_path);
            seed << "{\n"
                    "  \"model\": { \"default_model\": \"custom-model-x\" },\n"
                    "  \"display\": { \"theme\": \"dark\" },\n"
                    "  \"mcpServers\": {\n"
                    "    \"proj-srv\": {\n"
                    "      \"type\": \"stdio\",\n"
                    "      \"command\": \"run-proj\",\n"
                    "      \"args\": [\"--flag\"]\n"
                    "    }\n"
                    "  }\n"
                    "}\n";
        }
        {
            std::ofstream seed(user_path);
            seed << "{\n"
                    "  \"mcpServers\": {\n"
                    "    \"user-secret\": {\n"
                    "      \"type\": \"http\",\n"
                    "      \"url\": \"https://secrets.example.com/mcp\",\n"
                    "      \"headers\": { \"Authorization\": \"Bearer hunter2\" }\n"
                    "    }\n"
                    "  }\n"
                    "}\n";
        }
        const std::string user_bytes_before = read_file(user_path);

        cc::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set display.theme light", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;
        EXPECT_EQ(set->message, "Set display.theme = light");

        auto project = cc::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        const auto root_node = project->root();
        EXPECT_EQ(root_node.get("model").get("default_model").as_str(),
                  std::string_view("custom-model-x"));
        EXPECT_EQ(root_node.get("display").get("theme").as_str(),
                  std::string_view("light"));

        // The project-owned server survives with its OWN parsed value.
        const auto servers = root_node.get("mcpServers");
        ASSERT_TRUE(servers.is_obj());
        ASSERT_TRUE(servers.has("proj-srv"));
        const auto proj = servers.get("proj-srv");
        EXPECT_EQ(proj.get("type").as_str(), std::string_view("stdio"));
        EXPECT_EQ(proj.get("command").as_str(), std::string_view("run-proj"));

        // §B: the user-scope-only server name and its secret never enter the
        // tracked project file.
        const std::string project_bytes = read_file(project_path);
        EXPECT_FALSE(servers.has("user-secret"));
        EXPECT_EQ(project_bytes.find("user-secret"), std::string::npos);
        EXPECT_EQ(project_bytes.find("Bearer"), std::string::npos);
        EXPECT_EQ(project_bytes.find("Authorization"), std::string::npos);

        // The user tier file is never touched by a project save.
        EXPECT_EQ(read_file(user_path), user_bytes_before);
    }

    cleanup();
}

// RFC-0001 B followup c21: save() patches the KNOWN sections leaf-by-leaf, so
// unknown top-level keys (e.g. "x-custom") and unknown keys inside a known
// section (e.g. a custom "model" leaf) survive a /config set. Drives the REAL
// command path (AppCommandRegistry) like the c8/c19 suite.
TEST(AppCommandRegistry, ConfigSetPreservesUnknownKeys) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c21_unknown_");
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    EnvironmentUnsetGuard tokens_guard("LOOM_MAX_TOKENS");
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n"
                "  \"x-custom\": { \"tool\": \"loom\" },\n"
                "  \"model\": {\n"
                "    \"default_model\": \"seed-model\",\n"
                "    \"x_custom_leaf\": 42\n"
                "  },\n"
                "  \"display\": { \"theme\": \"dark\" }\n"
                "}\n";
    }

    {
        cc::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set display.theme light", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;

        auto project = cc::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        const auto root_node = project->root();
        // The unknown top-level key survives with its value intact.
        ASSERT_TRUE(root_node.has("x-custom"));
        EXPECT_EQ(root_node.get("x-custom").get("tool").as_str(),
                  std::string_view("loom"));
        // The unknown key inside "model" survives alongside the known leaves.
        const auto model = root_node.get("model");
        ASSERT_TRUE(model.is_obj());
        EXPECT_EQ(model.get("default_model").as_str(),
                  std::string_view("seed-model"));
        ASSERT_TRUE(model.has("x_custom_leaf"));
        EXPECT_EQ(model.get("x_custom_leaf").as_int(), 42);
        // The intended write landed.
        EXPECT_EQ(root_node.get("display").get("theme").as_str(),
                  std::string_view("light"));
    }

    cleanup();
}

// RFC-0001 B followup c23: the session-latched ConfigManager never re-read
// externally edited config, so a /config get after an external edit reported
// the stale snapshot (and a /config set would clobber the edit on save). With
// stat-based invalidation, tier_files_changed() detects the external edit
// (content hash + size/inode) and the second command reloads before reading.
TEST(AppCommandRegistry, ConfigGetReflectsExternalEdit) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c23_external_");
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    EnvironmentUnsetGuard model_guard("LOOM_MODEL");
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"model\": { \"default_model\": \"v1\" }\n}\n";
    }

    {
        cc::commands::AppCommandRegistry registry;

        auto get1 = registry.execute("/config get model.default_model", ctx());
        ASSERT_TRUE(get1.has_value());
        ASSERT_TRUE(get1->ok) << get1->message;
        EXPECT_EQ(get1->message, "model.default_model = v1");

        // External edit between commands.
        {
            std::ofstream edit(project_path, std::ios::trunc);
            edit << "{\n  \"model\": { \"default_model\": \"v2\" }\n}\n";
        }

        // Stat-based invalidation: the second command detects the external
        // edit (content hash change) and re-reads the file instead of serving
        // the latched "v1" snapshot.
        auto get2 = registry.execute("/config get model.default_model", ctx());
        ASSERT_TRUE(get2.has_value());
        ASSERT_TRUE(get2->ok) << get2->message;
        EXPECT_EQ(get2->message, "model.default_model = v2");
    }

    cleanup();
}

// c23: the McpCommand config latch invalidates on an external edit —
// autocomplete server-name suggestions must reflect the externally edited
// tier file instead of serving the latched snapshot. Drives complete(),
// which reads through ensure_config_loaded() without syncing the native
// runtime. A regression to `if (config_loaded_) return {};` makes the
// second half fail.
TEST(AppCommandRegistry, McpSuggestionsReflectExternalConfigEdit) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c23_mcp_");
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"mcpServers\": { \"alpha\": { \"command\": \"true\" } }\n}\n";
    }

    {
        // Construct AFTER env/cwd are set: the command's default ConfigManager
        // binds its paths in its constructor.
        cc::commands::McpCommand mcp;

        auto sug1 = mcp.complete("alpha");
        EXPECT_TRUE(std::ranges::find(sug1, "alpha") != sug1.end());

        // External edit between calls.
        {
            std::ofstream edit(project_path, std::ios::trunc);
            edit << "{\n  \"mcpServers\": {\n"
                    "    \"alpha\": { \"command\": \"true\" },\n"
                    "    \"beta\":  { \"command\": \"true\" }\n"
                    "  }\n}\n";
        }

        // The latch must invalidate: the second call re-reads the tier file.
        auto sug2 = mcp.complete("beta");
        EXPECT_TRUE(std::ranges::find(sug2, "beta") != sug2.end());
    }

    cleanup();
}

// RFC-0001 B followup c8: a hard parse error in the PROJECT file must fail
// /config set BEFORE any mutation or save — the command must never rewrite
// an unreadable file from default settings (nor leave a tmp file behind).
TEST(AppCommandRegistry, ConfigSetFailsOnUnreadableProjectConfig) {
    namespace fs = std::filesystem;
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_cmd_c8_unreadable_" + std::to_string(suffix));
    fs::remove_all(root);
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";

    {
        {
            std::ofstream seed(project_path);
            seed << "{ this is not valid json\n";
        }
        auto read_file = [](const fs::path& p) {
            std::ifstream in(p, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        };
        const std::string bytes_before = read_file(project_path);

        cc::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set display.theme dark", ctx());
        ASSERT_TRUE(set.has_value());
        EXPECT_FALSE(set->ok);
        EXPECT_FALSE(set->message.empty());

        // File bytes untouched, no atomic-write tmp, no new files in .loom.
        EXPECT_EQ(read_file(project_path), bytes_before);
        EXPECT_FALSE(fs::exists(work / ".loom" / "config.json.tmp"));
        std::error_code ec;
        EXPECT_EQ(std::distance(fs::directory_iterator(work / ".loom", ec),
                                fs::directory_iterator()), 1);
    }

    cleanup();
}

TEST(AppCommandRegistry, RuntimeSurfaceCommandsExecuteLocalLogic) {
    cc::commands::AppCommandRegistry registry;

    auto debug = registry.execute(R"(/debug-tool-call {"name":"Bash","input":{"command":"pwd"}})", ctx());
    ASSERT_TRUE(debug.has_value());
    EXPECT_TRUE(debug->ok);
    EXPECT_NE(debug->message.find("Tool-call payload valid"), std::string::npos);
    EXPECT_NE(debug->message.find("Tool: Bash"), std::string::npos);

    auto mock = registry.execute("/mock-limits 5", ctx());
    ASSERT_TRUE(mock.has_value());
    EXPECT_TRUE(mock->ok);
    EXPECT_NE(mock->message.find("Synthetic rate limit active"), std::string::npos);

    auto reset = registry.execute("/reset-limits all", ctx());
    ASSERT_TRUE(reset.has_value());
    EXPECT_TRUE(reset->ok);
    EXPECT_NE(reset->message.find("active=false"), std::string::npos);
    EXPECT_NE(reset->message.find("total_retries=0"), std::string::npos);

    auto bridge = registry.execute("/bridge status", ctx());
    ASSERT_TRUE(bridge.has_value());
    EXPECT_TRUE(bridge->ok);
    EXPECT_NE(bridge->message.find("Bridge status:"), std::string::npos);

    auto onboarding = registry.execute("/onboarding status", ctx());
    ASSERT_TRUE(onboarding.has_value());
    EXPECT_TRUE(onboarding->ok);
    EXPECT_NE(onboarding->message.find("Onboarding status"), std::string::npos);
}

TEST(AgentsCommand, ListsRealAgentDefinitions) {
    EnvironmentGuard explore_enabled("LOOM_ENABLE_EXPLORE_PLAN_AGENTS", "1");
    cc::commands::AgentsCommand agents;

    auto list = agents.execute(ctx({"list"}));
    ASSERT_TRUE(list.has_value());
    EXPECT_TRUE(list->ok);
    EXPECT_NE(list->message.find("Available agents:"), std::string::npos);
    EXPECT_NE(list->message.find("general-purpose"), std::string::npos);
    EXPECT_NE(list->message.find("Explore"), std::string::npos);
    EXPECT_EQ(list->message.find("default, fast, expert"), std::string::npos);

    auto details = agents.execute(ctx({"configure", "general-purpose"}));
    ASSERT_TRUE(details.has_value());
    EXPECT_TRUE(details->ok);
    EXPECT_NE(details->message.find("Agent: general-purpose"), std::string::npos);

    auto unknown = agents.execute(ctx({"use", "missing-agent"}));
    ASSERT_TRUE(unknown.has_value());
    EXPECT_FALSE(unknown->ok);
    EXPECT_NE(unknown->message.find("Unknown agent"), std::string::npos);
}

TEST(HelpCommand, FormatsDefaultShortcutsExamplesAndSpecificHelp) {
    auto help_def = cc::commands::HelpCommand::definition();
    cc::commands::HelpCommand help;
    help.set_command_definitions({&help_def});

    auto all = help.execute(ctx());
    ASSERT_TRUE(all.has_value());
    ASSERT_TRUE(all->metadata.has_value());
    EXPECT_EQ(*all->metadata, "UI:help");

    auto shortcuts = help.execute(ctx({"--shortcuts"}));
    ASSERT_TRUE(shortcuts.has_value());
    EXPECT_NE(shortcuts->message.find("Ctrl+C"), std::string::npos);

    auto examples = help.execute(ctx({"--examples"}));
    ASSERT_TRUE(examples.has_value());
    EXPECT_NE(examples->message.find("Examples"), std::string::npos);

    auto detailed = help.execute(ctx({"help"}));
    ASSERT_TRUE(detailed.has_value());
    EXPECT_NE(detailed->message.find("/help"), std::string::npos);
}

TEST(ClearCommand, InvokesCallbacksForAllScope) {
    cc::commands::ClearCommand clear;
    bool screen_cleared = false;
    bool conversation_reset = false;
    clear.set_screen_clear_fn([&] { screen_cleared = true; });
    clear.set_conversation_reset_fn([&]() -> cc::core::VoidResult {
        conversation_reset = true;
        return {};
    });

    auto result = clear.execute(ctx({"--all"}));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ok);
    EXPECT_TRUE(screen_cleared);
    EXPECT_TRUE(conversation_reset);
    EXPECT_NE(result->message.find("Screen cleared"), std::string::npos);
    EXPECT_NE(result->message.find("Conversation reset"), std::string::npos);
}

TEST(CompactCommand, RuntimeContextCompactsActiveQueryEngineConversation) {
    cc::core::ToolRegistry tool_registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = std::filesystem::current_path().string();
    cc::core::QueryEngine engine(std::move(config), tool_registry);

    auto make_user = [](std::string text, int index) {
        cc::core::UserMessage msg{};
        msg.id.value = "compact-user-" + std::to_string(index);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 10; ++i) {
        engine.append_message_for_testing(make_user(
            "retain compact command runtime detail " + std::to_string(i) + " " +
            std::string(400, static_cast<char>('a' + (i % 20))),
            i));
    }

    auto before = engine.get_conversation();
    ASSERT_GT(before.size(), 8u);

    cc::commands::AppCommandRegistry registry;
    auto command_ctx = ctx();
    command_ctx.runtime_state = &engine;
    command_ctx.compact_message_provider = compact_runtime_messages;
    command_ctx.compact_applier = compact_runtime_apply;
    auto result = registry.execute("/compact --target 20", command_ctx);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ok);
    EXPECT_EQ(result->status, cc::core::CommandStatus::Succeeded);
    EXPECT_NE(result->message.find("Compaction complete"), std::string::npos);
    EXPECT_EQ(result->message.find("Summarize the following conversation segments"), std::string::npos);

    auto after = engine.get_conversation();
    EXPECT_LT(after.size(), before.size());
    ASSERT_GT(after.size(), 1u);
    bool found_summary_marker = false;
    for (const auto& message : after) {
        const auto* marker = std::get_if<cc::core::UserMessage>(&message);
        if (!marker || marker->content.empty()) continue;
        const auto* text = std::get_if<cc::core::TextBlock>(&marker->content.front());
        if (text && text->text.find("Preserve these details") != std::string::npos) {
            found_summary_marker = true;
            break;
        }
    }
    EXPECT_TRUE(found_summary_marker);
}

TEST(ConfigCommand, ValidatesRequiredArgumentsAndListsConfig) {
    cc::commands::ConfigCommand config;

    EXPECT_TRUE(config.validate(ctx({"list"})).has_value());
    EXPECT_FALSE(config.validate(ctx({"get"})).has_value());
    EXPECT_FALSE(config.validate(ctx({"set", "model.default_model"})).has_value());

    auto list = config.execute(ctx({"list"}));
    ASSERT_TRUE(list.has_value());
    EXPECT_NE(list->message.find("Current Configuration"), std::string::npos);

    auto completions = config.complete("pa");
    ASSERT_FALSE(completions.empty());
    EXPECT_EQ(completions.front(), "path");
}

TEST(ModelCommand, ListsSwitchesAndCompletesModels) {
    cc::commands::ModelCommand model;

    auto list = model.execute(ctx({"list"}));
    ASSERT_TRUE(list.has_value());
    EXPECT_NE(list->message.find("claude-sonnet-4-20250514"), std::string::npos);

    auto switched = model.execute(ctx({"set", "claude-opus-4-20250514"}));
    ASSERT_TRUE(switched.has_value());
    EXPECT_NE(switched->message.find("claude-opus-4-20250514"), std::string::npos);

    auto invalid = model.validate(ctx({"not-a-model"}));
    EXPECT_FALSE(invalid.has_value());

    auto completions = model.complete("claude-");
    EXPECT_GE(completions.size(), 3u);
}

TEST(MigratedCommandMetadata, HooksAndRewindExposeTypeScriptCompatibleMetadata) {
    auto hooks_def = cc::commands::HooksCommand::definition();
    auto rewind_def = cc::commands::RewindCommand::definition();

    EXPECT_EQ(hooks_def.name, "hooks");
    EXPECT_EQ(hooks_def.description, "View hook configurations for tool events");
    EXPECT_FALSE(hooks_def.hidden);

    ASSERT_EQ(rewind_def.aliases.size(), 1u);
    EXPECT_EQ(rewind_def.aliases.front(), "checkpoint");
}

TEST(MigratedCommands, ExecuteActionableMessagesAndCompletions) {
    cc::commands::HooksCommand hooks;
    auto hooks_result = hooks.execute(ctx({"list"}));
    ASSERT_TRUE(hooks_result.has_value());
    EXPECT_NE(hooks_result->message.find("PreToolUse"), std::string::npos);

    cc::commands::RewindCommand rewind;
    auto rewind_result = rewind.execute(ctx({"code"}));
    ASSERT_TRUE(rewind_result.has_value());
    EXPECT_NE(rewind_result->message.find("code"), std::string::npos);

    auto rewind_completions = rewind.complete("c");
    ASSERT_EQ(rewind_completions.size(), 2u);
    EXPECT_EQ(rewind_completions.front(), "conversation");
}

// ============================================================================
// Insights facet pipeline — deterministic tests for the portable core
// (label map, aggregation, cache round-trip, HTML report structure, seam).
// ============================================================================

namespace {

namespace insights = cc::commands;
namespace fs = std::filesystem;

// Scoped HOME override so facet cache tests never touch the real user store.
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

insights::SessionFacets make_facets(std::string id) {
    insights::SessionFacets f;
    f.session_id = std::move(id);
    f.underlying_goal = "Ship the feature";
    f.goal_categories = {{"implement_feature", 2}, {"fix_bug", 1}};
    f.outcome = "fully_achieved";
    f.user_satisfaction_counts = {{"satisfied", 2}, {"happy", 1}};
    f.loom_helpfulness = "very_helpful";
    f.session_type = "iterative_refinement";
    f.friction_counts = {{"buggy_code", 1}};
    f.friction_detail = "One bad edit";
    f.primary_success = "correct_code_edits";
    f.brief_summary = "User shipped the feature";
    return f;
}

} // namespace

TEST(Insights, HumaniseLabelMap) {
    EXPECT_EQ(insights::humanise("fix_bug"), "Fix Bug");
    EXPECT_EQ(insights::humanise("fully_achieved"), "Fully Achieved");
    EXPECT_EQ(insights::humanise("satisfied"), "Satisfied");
    // Unknown keys pass through verbatim (matches TS behaviour).
    EXPECT_EQ(insights::humanise("unknown_thing"), "unknown_thing");
}

TEST(Insights, AggregateFacetsIsDeterministic) {
    std::vector<insights::SessionFacets> facets = {
        make_facets("sess-a"),
        make_facets("sess-b"),
    };
    auto agg = insights::aggregate_facets(facets);
    EXPECT_EQ(agg.sessions_with_facets, 2u);
    EXPECT_EQ(agg.goal_categories.at("implement_feature"), 4u);
    EXPECT_EQ(agg.goal_categories.at("fix_bug"), 2u);
    EXPECT_EQ(agg.outcomes.at("fully_achieved"), 2u);
    EXPECT_EQ(agg.satisfaction.at("satisfied"), 4u);
    EXPECT_EQ(agg.satisfaction.at("happy"), 2u);
    EXPECT_EQ(agg.helpfulness.at("very_helpful"), 2u);
    EXPECT_EQ(agg.session_types.at("iterative_refinement"), 2u);
    EXPECT_EQ(agg.friction.at("buggy_code"), 2u);
    EXPECT_EQ(agg.success.at("correct_code_edits"), 2u);
    // 'none' primary_success is excluded.
    EXPECT_EQ(agg.success.count("none"), 0u);
}

TEST(Insights, AggregateFacetsSkipsNonPositiveCounts) {
    auto f = make_facets("sess-c");
    // Zero/negative counts must be filtered (matches TS `count > 0`).
    f.goal_categories["zero_goal"] = 0;
    auto agg = insights::aggregate_facets({f});
    EXPECT_EQ(agg.goal_categories.count("zero_goal"), 0u);
    EXPECT_EQ(agg.goal_categories.at("implement_feature"), 2u);
}

TEST(Insights, AggregateFacetsEmpty) {
    auto agg = insights::aggregate_facets({});
    EXPECT_EQ(agg.sessions_with_facets, 0u);
    EXPECT_TRUE(agg.goal_categories.empty());
}

TEST(Insights, FacetCacheRoundTrip) {
    HomeGuard guard;
    auto original = make_facets("round-trip-1");
    auto save_res = insights::save_facets(original);
    ASSERT_TRUE(save_res.has_value()) << save_res.error().message();

    auto loaded = insights::load_facets("round-trip-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->session_id, original.session_id);
    EXPECT_EQ(loaded->underlying_goal, original.underlying_goal);
    EXPECT_EQ(loaded->outcome, original.outcome);
    EXPECT_EQ(loaded->goal_categories, original.goal_categories);
    EXPECT_EQ(loaded->user_satisfaction_counts,
              original.user_satisfaction_counts);
    EXPECT_EQ(loaded->friction_counts, original.friction_counts);
    EXPECT_EQ(loaded->primary_success, original.primary_success);
    EXPECT_EQ(loaded->brief_summary, original.brief_summary);
}

TEST(Insights, FacetCacheMissForUnknownSession) {
    HomeGuard guard;
    auto loaded = insights::load_facets("does-not-exist-xyz");
    EXPECT_FALSE(loaded.has_value());
}

TEST(Insights, FacetCacheRejectsInvalidSchema) {
    HomeGuard guard;
    // Missing required string fields + missing required object fields.
    const std::string bad = R"({"session_id":"x","foo":1})";
    auto path = insights::detail::facets_dir();
    fs::create_directories(path);
    { std::ofstream o(path / "bad-1.json"); o << bad; }
    EXPECT_FALSE(insights::load_facets("bad-1").has_value());
}

TEST(Insights, FacetCacheAcceptsTSJsonShape) {
    HomeGuard guard;
    // A facets JSON shaped exactly like the TS cache format.
    const std::string good = R"({
      "session_id": "ts-1",
      "underlying_goal": "Goal",
      "goal_categories": {"fix_bug": 3, "implement_feature": 1},
      "outcome": "mostly_achieved",
      "user_satisfaction_counts": {"satisfied": 2},
      "loom_helpfulness": "essential",
      "session_type": "multi_task",
      "friction_counts": {"wrong_approach": 1},
      "friction_detail": "Detour",
      "primary_success": "multi_file_changes",
      "brief_summary": "Did the thing"
    })";
    auto path = insights::detail::facets_dir();
    fs::create_directories(path);
    { std::ofstream o(path / "ts-1.json"); o << good; }

    auto loaded = insights::load_facets("ts-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->goal_categories.at("fix_bug"), 3u);
    EXPECT_EQ(loaded->outcome, "mostly_achieved");
    EXPECT_EQ(loaded->loom_helpfulness, "essential");
}

TEST(Insights, HtmlReportHasExpectedStructure) {
    std::vector<insights::SessionFacets> facets = {make_facets("h1")};
    auto agg = insights::aggregate_facets(facets);
    auto html = insights::render_html_report(
        5, 100, "2026-01-01", "2026-06-01", agg);

    EXPECT_TRUE(html.starts_with("<!DOCTYPE html>"));
    EXPECT_NE(html.find("<title>Loom Insights</title>"), std::string::npos);
    // Sections present.
    EXPECT_NE(html.find("id=\"goals\""), std::string::npos);
    EXPECT_NE(html.find("id=\"outcomes\""), std::string::npos);
    EXPECT_NE(html.find("id=\"satisfaction\""), std::string::npos);
    EXPECT_NE(html.find("id=\"friction\""), std::string::npos);
    // Humanised label rendered (Fix Bug), not the raw key.
    EXPECT_NE(html.find("Fix Bug"), std::string::npos);
    EXPECT_EQ(html.find("fix_bug"), std::string::npos);
    // Closing tags.
    EXPECT_NE(html.find("</html>"), std::string::npos);
}

TEST(Insights, HtmlReportEscapesAndHumanises) {
    // HTML-escape coverage: render with a value known to contain HTML. The
    // date-range subtitle passes straight into the document, so inject markup
    // there and assert it is escaped (no raw tag survives).
    insights::AggregatedFacets agg;
    agg.sessions_with_facets = 1;
    agg.goal_categories = {{"fix_bug", 1}};
    const std::string evil_date = "2026<script>boom</script>";
    auto html = insights::render_html_report(1, 1, evil_date, "2026-01-02", agg);
    // Raw angle brackets from the date value must be escaped.
    EXPECT_EQ(html.find("<script>boom"), std::string::npos);
    EXPECT_NE(html.find("&lt;script&gt;boom&lt;/script&gt;"), std::string::npos);
}

TEST(Insights, ExtractFacetsSeamParsesValidJson) {
    // The seam receives a fully-formed prompt+transcript (the live caller
    // prepends facet_extraction_prompt()). Assert it is forwarded intact.
    const std::string transcript =
        cc::commands::facet_extraction_prompt() + "user: please fix the bug";
    cc::commands::LlmExtractFn stub = [&transcript](
                                          const std::string& received) {
        EXPECT_EQ(received, transcript);
        return std::optional<std::string>(
            std::string("Here you go: {") +
            R"("underlying_goal":"g","goal_categories":{"fix_bug":2},)"
            R"("outcome":"fully_achieved","user_satisfaction_counts":{"happy":1},)"
            R"("loom_helpfulness":"very_helpful","session_type":"single_task",)"
            R"("friction_counts":{"buggy_code":1},"friction_detail":"",)"
            R"("primary_success":"none","brief_summary":"b"}) done.)");
    };
    auto f = cc::commands::extract_facets_with_seam(stub, transcript, "sid");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->session_id, "sid");
    EXPECT_EQ(f->underlying_goal, "g");
    EXPECT_EQ(f->goal_categories.at("fix_bug"), 2u);
    EXPECT_EQ(f->outcome, "fully_achieved");
}

TEST(Insights, ExtractFacetsSeamRejectsInvalidSchema) {
    cc::commands::LlmExtractFn stub = [](const std::string&) {
        // Object present but missing required fields.
        return std::optional<std::string>(std::string(R"({"foo":1})"));
    };
    auto f = cc::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamRejectsNoJson) {
    cc::commands::LlmExtractFn stub = [](const std::string&) {
        return std::optional<std::string>(std::string("no json here at all"));
    };
    auto f = cc::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamHandlesNullFn) {
    cc::commands::LlmExtractFn stub;  // empty
    auto f = cc::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamHandlesEmptyResponse) {
    cc::commands::LlmExtractFn stub = [](const std::string&) {
        return std::optional<std::string>(std::string(""));
    };
    auto f = cc::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, FacetExtractionPromptContainsGuidelines) {
    const auto& p = cc::commands::facet_extraction_prompt();
    // The prompt must mention the key contract elements from the TS source.
    EXPECT_NE(p.find("goal_categories"), std::string::npos);
    EXPECT_NE(p.find("user_satisfaction_counts"), std::string::npos);
    EXPECT_NE(p.find("friction_counts"), std::string::npos);
    EXPECT_NE(p.find("warmup_minimal"), std::string::npos);
    EXPECT_NE(p.find("SESSION:"), std::string::npos);
}

TEST(Insights, CommandDefinitionAndModel) {
    auto def = cc::commands::InsightsCommand::definition();
    EXPECT_EQ(def.name, "insights");
    EXPECT_FALSE(def.hidden);
    EXPECT_EQ(cc::commands::InsightsCommand::default_analysis_model(),
              "claude-opus-4-20250514");
}

TEST(Insights, CommandExecuteWithNoSessionsIsGraceful) {
    HomeGuard guard;
    // Point HOME at an empty dir with no sessions subdir -> list_sessions empty.
    cc::commands::InsightsCommand cmd;
    cc::core::CommandContext cctx{};
    auto res = cmd.execute(cctx);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->ok);
    EXPECT_NE(res->message.find("No sessions found"), std::string::npos);
}

// ============================================================================
// Plugin command — behavioural parity with TS src/commands/plugin/.
// TS REF: PluginSettings.tsx getInitialViewState / PluginSettings render.
// Interactive subcommands open the tabbed dialog with NO intermediate text;
// they only emit the "UI:plugins:<view>" spawn metadata.
// ============================================================================

TEST(PluginCommand, DefinitionExposesNameAliasesAndCategory) {
    auto def = cc::commands::PluginCommand::definition();
    EXPECT_EQ(def.name, "plugin");
    EXPECT_EQ(def.category, "tools");
    ASSERT_EQ(def.aliases.size(), 2u);
    EXPECT_EQ(def.aliases[0], "plugins");
    EXPECT_EQ(def.aliases[1], "marketplace");
    EXPECT_FALSE(def.hidden);
}

TEST(PluginCommand, ValidateAcceptsKnownSubcommandsAndRejectsUnknown) {
    cc::commands::PluginCommand cmd;
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
    cc::commands::PluginCommand cmd;
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
    cc::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"nope"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:discover-plugins");
}

TEST(PluginCommand, HelpUsesHyphensNotEmDashesLikeTypeScript) {
    cc::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"help"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r->message.find("Plugin Command Usage"), std::string::npos);
    // TS help text uses plain hyphens; em-dashes (—) would be a divergence.
    EXPECT_EQ(r->message.find("\xe2\x80\x94"), std::string::npos);
    EXPECT_NE(r->message.find("- Browse and install plugins"), std::string::npos);
}

TEST(PluginCommand, HelpHelpAliasAndFlagsAllRenderHelp) {
    cc::commands::PluginCommand cmd;
    for (const auto& tok : {"help", "--help", "-h"}) {
        auto r = cmd.execute(ctx({tok}));
        ASSERT_TRUE(r.has_value());
        EXPECT_NE(r->message.find("Plugin Command Usage"), std::string::npos);
    }
}

TEST(PluginCommand, ManageOpensInstalledTabWithNoTextList) {
    HomeGuard guard;
    cc::commands::PluginCommand cmd;
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
    cc::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"install"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:discover-plugins");
}

TEST(PluginCommand, InstallMarketplaceOpensBrowseScopedToMarketplace) {
    HomeGuard guard;
    cc::commands::PluginCommand cmd;
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
    namespace pp = cc::commands::plugin;
    auto parsed = pp::parse_plugin_args("install my-cool-plugin");
    EXPECT_EQ(parsed.type, pp::SubcommandType::Install);
    EXPECT_TRUE(parsed.plugin_name.has_value());
    EXPECT_EQ(*parsed.plugin_name, "my-cool-plugin");
    EXPECT_FALSE(parsed.marketplace.has_value());
}

TEST(PluginCommand, ValidateWithNoPathPrintsUsage) {
    cc::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"validate"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r->message.find("Usage: /plugin validate <path>"), std::string::npos);
}

TEST(PluginCommand, MarketplaceNoActionOpensMarketplacesTab) {
    HomeGuard guard;
    cc::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"marketplace"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:manage-marketplaces");
}

TEST(PluginCommand, EnableDisableUninstallWithoutTargetRouteToInstalledTab) {
    HomeGuard guard;
    cc::commands::PluginCommand cmd;
    for (const auto& tok : {"enable", "disable", "uninstall"}) {
        auto r = cmd.execute(ctx({tok}));
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(r->metadata.has_value());
        EXPECT_EQ(r->metadata->find("UI:plugins:manage-plugins"), 0u);
    }
}

TEST(PluginCommand, CompletionSuggestsSubcommands) {
    cc::commands::PluginCommand cmd;
    auto c = cmd.complete("in");
    EXPECT_NE(std::find(c.begin(), c.end(), "install"), c.end());
}

// ============================================================================
// terminal-setup OSC 8 hyperlink support
// TS REF: src/commands/terminalSetup/terminalSetup.tsx L54-72 formatPathLink()
// ============================================================================

namespace {

namespace fs = std::filesystem;

// RAII HOME override for terminal-setup tests; mirrors HomeGuard above but
// with its own temp prefix/label so the suites stay independent if co-located.
struct TerminalSetupHomeGuard {
    std::optional<std::string> previous;
    fs::path tmp;
    explicit TerminalSetupHomeGuard() {
        if (const char* h = std::getenv("HOME")) previous = h;
        auto base = fs::temp_directory_path();
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        static std::atomic<long> counter{0};
        tmp = base / ("cc-termsetup-test-" + std::to_string(stamp) + "-" +
                      std::to_string(counter.fetch_add(1)));
        fs::create_directories(tmp);
        setenv("HOME", tmp.c_str(), 1);
    }
    ~TerminalSetupHomeGuard() {
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

TEST(TerminalSetupCommand, PreviewAndApplyWrapDisplayPathsInOsc8Links) {
    TerminalSetupHomeGuard home;
    EnvironmentGuard term_program("TERM_PROGRAM", "vscode");
    EnvironmentUnsetGuard wt_session("WT_SESSION");
    EnvironmentUnsetGuard vte_version("VTE_VERSION");

    namespace ts = cc::commands::terminal_setup;
    const std::string rc = (home.tmp / ".zshrc").string();

    // 1) Preview mode: header path is hyperlinked but plain text still present.
    auto r1 = ts::run("--shell=zsh");
    ASSERT_TRUE(r1.ok);
    EXPECT_NE(r1.message.find("RC file  : \x1b]8;;file://" + rc),
              std::string::npos);
    EXPECT_NE(r1.message.find(rc), std::string::npos);

    // Pre-create the rc file so apply takes the backup-existing-file path.
    {
        std::ofstream pre(rc, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(pre.is_open());
        pre << "# preexisting user content\n";
    }

    // 2) Apply mode: rc path, backup path linked; source instruction stays plain.
    auto r2 = ts::run("--apply --shell=zsh");
    ASSERT_TRUE(r2.ok);
    // OSC8 opener with ST = ESC-backslash.
    EXPECT_NE(r2.message.find("\x1b]8;;file://" + rc + "\x1b\\"),
              std::string::npos);
    // Plain path human-readable between opener and closer.
    EXPECT_NE(r2.message.find(rc), std::string::npos);
    // Closing OSC8 marker.
    EXPECT_NE(r2.message.find("\x1b]8;;\x1b\\"), std::string::npos);
    EXPECT_NE(r2.message.find("Successfully applied terminal-setup snippet"),
              std::string::npos);
    EXPECT_NE(r2.message.find("Backup created at:"), std::string::npos);
    // Backup path is also linked (timestamp suffix means prefix match).
    EXPECT_NE(r2.message.find("\x1b]8;;file://" +
                              (home.tmp / ".zshrc.bak-").string()),
              std::string::npos);
    // The executable `source <rc>` instruction must stay plain: no ESC bytes
    // between "source " and the rc path.
    EXPECT_NE(r2.message.find("  source " + rc + "\n"), std::string::npos);

    // The snippet must actually have been written to disk.
    std::ifstream ifs(rc, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::string on_disk((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(on_disk.find("# >>> loom terminal-setup"), std::string::npos);
}

TEST(TerminalSetupCommand, HyperlinkGateMatchesTsTerminalMatrix) {
    // TS REF: src/ink/supports-hyperlinks.ts — the ADDITIONAL whitelist via
    // TERM_PROGRAM and LC_TERMINAL, plus TERM containing "kitty".
    namespace cu = cc::utils;
    auto with_env = [](std::initializer_list<std::pair<const char*, const char*>> set,
                       std::initializer_list<const char*> unset) {
        std::vector<std::unique_ptr<EnvironmentGuard>> guards;
        std::vector<std::unique_ptr<EnvironmentUnsetGuard>> unsets;
        for (auto [k, v] : set)
            guards.push_back(std::make_unique<EnvironmentGuard>(k, v));
        for (auto k : unset)
            unsets.push_back(std::make_unique<EnvironmentUnsetGuard>(k));
        return cu::supports_hyperlinks();
    };

    // TERM_PROGRAM whitelist (TS ADDITIONAL_HYPERLINK_TERMINALS + base set).
    for (const char* prog :
         {"ghostty", "Hyper", "kitty", "alacritty", "iTerm.app", "iTerm2",
          "WezTerm", "vscode"}) {
        EXPECT_TRUE(with_env({{"TERM_PROGRAM", prog}},
                             {"WT_SESSION", "VTE_VERSION", "TERM", "LC_TERMINAL"}))
            << "TERM_PROGRAM=" << prog;
    }
    // LC_TERMINAL (preserved inside tmux where TERM_PROGRAM becomes tmux).
    EXPECT_TRUE(with_env({{"LC_TERMINAL", "iTerm2"},
                          {"TERM_PROGRAM", "tmux"}},
                         {"WT_SESSION", "VTE_VERSION", "TERM"}));
    // TERM contains kitty.
    EXPECT_TRUE(with_env({{"TERM", "xterm-kitty"}},
                         {"TERM_PROGRAM", "WT_SESSION", "VTE_VERSION",
                          "LC_TERMINAL"}));
    // Unknown terminal stays off.
    EXPECT_FALSE(with_env({},
                          {"TERM_PROGRAM", "WT_SESSION", "VTE_VERSION",
                           "TERM", "LC_TERMINAL"}));
    EXPECT_FALSE(with_env({{"TERM_PROGRAM", "dumb"}},
                          {"WT_SESSION", "VTE_VERSION", "TERM", "LC_TERMINAL"}));
}

TEST(TerminalSetupCommand, UnsupportedTerminalEmitsBarePathsWithoutEscapes) {
    TerminalSetupHomeGuard home;
    EnvironmentUnsetGuard term_program("TERM_PROGRAM");
    EnvironmentUnsetGuard wt_session("WT_SESSION");
    EnvironmentUnsetGuard vte_version("VTE_VERSION");

    // Guard sanity: with every recognized variable cleared the gate is false.
    EXPECT_FALSE(cc::utils::supports_hyperlinks());

    namespace ts = cc::commands::terminal_setup;
    auto r = ts::run("--apply --shell=bash");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.message.find("\x1b]8;;"), std::string::npos);
    EXPECT_EQ(r.message.find("\x1b"), std::string::npos);
    const std::string rc = (home.tmp / ".bashrc").string();
    EXPECT_NE(r.message.find(rc), std::string::npos);
    // Header line is the bare path immediately followed by newline.
    EXPECT_NE(r.message.find("RC file  : " + rc + "\n"), std::string::npos);
}

TEST(TerminalSetupCommand, PathToFileUrlEncodingAndHyperlinkGate) {
    namespace cu = cc::utils;
    // TS REF: Node url.pathToFileURL — '/' and the Node safe set pass through;
    // every other byte is uppercased percent-encoded (UTF-8 bytes for non-ASCII).
    // Safe set verified empirically against Node v22: '[' ']' ARE encoded
    // (%5B/%5D) while '&' stays raw.
    EXPECT_EQ(cu::path_to_file_url("/home/u/.zshrc"),
              "file:///home/u/.zshrc");
    EXPECT_EQ(cu::path_to_file_url("/home/u/a b/keymap.json"),
              "file:///home/u/a%20b/keymap.json");
    EXPECT_EQ(cu::path_to_file_url("/tmp/a#b?c%&[1].fish"),
              "file:///tmp/a%23b%3Fc%25&%5B1%5D.fish");
    EXPECT_EQ(cu::path_to_file_url("/tmp/caf\xC3\xA9.rc"),
              "file:///tmp/caf%C3%A9.rc");
    // Tilde is encoded by Node (not in the unreserved set).
    EXPECT_EQ(cu::path_to_file_url("/tmp/a~b"),
              "file:///tmp/a%7Eb");

    const std::string url = "file:///home/u/.zshrc";
    const std::string text = "/home/u/.zshrc";
    {
        EnvironmentGuard term_program("TERM_PROGRAM", "vscode");
        EnvironmentUnsetGuard wt_session("WT_SESSION");
        EnvironmentUnsetGuard vte_version("VTE_VERSION");
        const std::string linked = cu::make_hyperlink(url, text);
        EXPECT_EQ(linked.rfind("\x1b]8;;" + url + "\x1b\\", 0), 0u);
        EXPECT_NE(linked.find(text), std::string::npos);
        // Closing OSC8 marker is 7 bytes: \x1b]8;; (5) + \x1b\ (2).
        EXPECT_EQ(linked.compare(linked.size() - 7, 7, "\x1b]8;;\x1b\\"), 0);
    }
    {
        EnvironmentUnsetGuard term_program("TERM_PROGRAM");
        EnvironmentUnsetGuard wt_session("WT_SESSION");
        EnvironmentUnsetGuard vte_version("VTE_VERSION");
        EXPECT_EQ(cu::make_hyperlink(url, text), text);
    }
}

// RFC-0001 B followup c17a — END-TO-END: `/mcp xaa setup --callback-port`
// persists settings.xaaIdp.callbackPort (config.json), and the SAME store is
// what the XAA login path resolves its fixed loopback port from. Drives the
// real AppCommandRegistry (temp HOME / LOOM_CONFIG_DIR / CWD), then asserts
// the persisted value flows through build_login_options() — the exact seam
// acquire_idp_id_token() consumes. No OIDC discovery, no socket, no browser.
//
// This is the counter-test for FINDING 1: before c17a the CLI-writable store
// did not reach the runtime path, and c17's added surface was a second,
// unwritable store.
TEST(AppCommandRegistry, XaaSetupCallbackPortReachesLoginSeam) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c17a_xaa_");
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work);

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    {
        cc::commands::AppCommandRegistry registry;

        auto setup = registry.execute(
            "/mcp xaa setup --issuer https://idp.example.com "
            "--client-id loom-cli --callback-port 19485",
            ctx());
        ASSERT_TRUE(setup.has_value());
        ASSERT_TRUE(setup->ok) << setup->message;

        // The value landed in the SUPPORTED store (config.json / xaaIdp), not
        // in a separate file. Default save target is the project tier.
        auto project = cc::utils::json::parse_file(work / ".loom" / "config.json");
        ASSERT_TRUE(project.has_value());
        const auto xaa = project->root().get("xaaIdp");
        ASSERT_TRUE(xaa.is_obj());
        EXPECT_EQ(xaa.get("issuer").as_str(), std::string_view("https://idp.example.com"));
        EXPECT_EQ(xaa.get("clientId").as_str(), std::string_view("loom-cli"));
        ASSERT_TRUE(xaa.get("callbackPort").is_num());
        EXPECT_EQ(xaa.get("callbackPort").as_int(), 19485);

        // The port reaches the login seam that acquire_idp_id_token() binds
        // from: the resolved port is the configured one, not a random port.
        auto opts = cc::services::mcp::build_login_options(
            "https://idp.example.com", "loom-cli",
            std::optional<int>{static_cast<int>(xaa.get("callbackPort").as_int())});
        auto resolved = cc::services::mcp::resolve_login_callback_port(opts);
        ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
        ASSERT_TRUE(resolved->has_value());
        EXPECT_EQ(**resolved, 19485u);
    }

    cleanup();
}

// RFC-0001 B followup c19: an UNRELATED /config set must not bake the
// ephemeral LOOM_MODEL / LOOM_MAX_TOKENS env values into the VCS-tracked
// project file. Drives the REAL command path (AppCommandRegistry) with both
// env vars exported. Test 1 of the c19 command suite.
TEST(AppCommandRegistry, ConfigSetDoesNotBakeEnvModelIntoProjectFile) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c19_envbake_");
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    // The bug's trigger: an ephemeral model/token exported by the user.
    EnvironmentGuard model_guard("LOOM_MODEL", "env-ephemeral-model-c19");
    EnvironmentGuard tokens_guard("LOOM_MAX_TOKENS", "4321");

    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);
    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";

    {
        {
            std::ofstream seed(project_path);
            seed << "{\n"
                    "  \"model\": { \"default_model\": \"file-model-c19\" },\n"
                    "  \"display\": { \"theme\": \"light\" }\n"
                    "}\n";
        }

        // Construct AFTER env + cwd: the command's manager binds paths in its
        // ctor and reads the env on load().
        cc::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set display.theme dark", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;

        const std::string bytes = cmd_read_file(project_path);
        // The unrelated key landed...
        EXPECT_NE(bytes.find("\"theme\": \"dark\""), std::string::npos) << bytes;
        // ...while the env value was never written into the tracked file.
        EXPECT_EQ(bytes.find("env-ephemeral-model-c19"), std::string::npos)
            << bytes;
        EXPECT_EQ(bytes.find("4321"), std::string::npos) << bytes;
        // The file's OWN model value is preserved (not dropped, not env).
        auto project = cc::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        EXPECT_EQ(project->root().get("model").get("default_model").as_str(),
                  std::string_view("file-model-c19"));
    }

    // The real proof it was not baked: with the env UNSET, a fresh load
    // yields the file's own model values, not the ephemeral ones.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        EnvironmentUnsetGuard unset_tokens("LOOM_MAX_TOKENS");
        cc::core::ConfigManager reloaded(work / ".loom" / "config.json",
                                         project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_EQ(reloaded.settings().model.default_model, "file-model-c19");
        EXPECT_NE(reloaded.settings().model.max_output_tokens, 4321u);
    }

    cleanup();
}

// RFC-0001 B followup c19: pin the serializer's EXISTING exclusion of the
// network secret/endpoint values at the byte level, so a future serializer
// change cannot silently start leaking an exported ANTHROPIC_API_KEY /
// ANTHROPIC_BASE_URL / proxy into the tracked project file. Test 2.
TEST(AppCommandRegistry, ConfigSetNeverWritesEnvCredentialsToFile) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c19_secrets_");
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    EnvironmentGuard api_guard("ANTHROPIC_API_KEY", "SECRET-c19-traceable-key");
    EnvironmentGuard base_guard("ANTHROPIC_BASE_URL",
                                "https://SECRET-c19-base.example.invalid/api");
    EnvironmentGuard proxy_guard("HTTPS_PROXY",
                                 "http://SECRET-c19-proxy.example:3128");

    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);
    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"display\": { \"theme\": \"light\" }\n}\n";
    }

    {
        cc::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set display.theme dark", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;

        const std::string bytes = cmd_read_file(project_path);
        // No transport of the exported credential/endpoint bytes anywhere.
        EXPECT_EQ(bytes.find("SECRET-c19"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("api_key"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("base_url"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("proxy"), std::string::npos) << bytes;
        // The intended write is still there.
        EXPECT_NE(bytes.find("\"theme\": \"dark\""), std::string::npos) << bytes;
    }

    cleanup();
}

// RFC-0001 B followup c19: an EXPLICIT /config set of an env-overridden leaf
// is user intent and MUST persist, even while the env var is exported. The
// runtime effective value stays env-overridden until the env is unset — the
// documented (and asserted) behavior. Test 3 + 4 (both model leaves).
TEST(AppCommandRegistry, ConfigSetExplicitEnvOverriddenLeafPersistsUserValue) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c19_explicit_");
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    EnvironmentGuard model_guard("LOOM_MODEL", "Y-env-model-c19");
    EnvironmentGuard tokens_guard("LOOM_MAX_TOKENS", "9999");

    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);
    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    const fs::path project_path = work / ".loom" / "config.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"model\": { \"default_model\": \"file-seed-c19\" }\n}\n";
    }

    {
        cc::commands::AppCommandRegistry registry;

        // Explicit user intent while LOOM_MODEL=Y: X wins on disk.
        auto set_model = registry.execute(
            "/config set model.default_model X-user-model-c19", ctx());
        ASSERT_TRUE(set_model.has_value());
        ASSERT_TRUE(set_model->ok) << set_model->message;
        {
            const std::string bytes = cmd_read_file(project_path);
            EXPECT_NE(bytes.find("X-user-model-c19"), std::string::npos) << bytes;
            EXPECT_EQ(bytes.find("Y-env-model-c19"), std::string::npos) << bytes;
            auto project = cc::utils::json::parse_file(project_path);
            ASSERT_TRUE(project.has_value());
            EXPECT_EQ(project->root().get("model").get("default_model").as_str(),
                      std::string_view("X-user-model-c19"));
        }

        // Same rule for model.max_output_tokens with LOOM_MAX_TOKENS engaged.
        auto set_tokens = registry.execute(
            "/config set model.max_output_tokens 2048", ctx());
        ASSERT_TRUE(set_tokens.has_value());
        ASSERT_TRUE(set_tokens->ok) << set_tokens->message;
        {
            const std::string bytes = cmd_read_file(project_path);
            EXPECT_NE(bytes.find("\"max_output_tokens\": 2048"), std::string::npos)
                << bytes;
            EXPECT_EQ(bytes.find("9999"), std::string::npos) << bytes;
        }

        // Documented runtime behavior: the env overlay applies at LOAD time,
        // so within THIS manager the explicit set is what /config get reports
        // (the in-memory value is the user's X). A FRESH load while the env
        // is still exported re-applies the overlay and yields Y — see below.
        auto get_model = registry.execute("/config get model.default_model", ctx());
        ASSERT_TRUE(get_model.has_value());
        EXPECT_EQ(get_model->message, "model.default_model = X-user-model-c19");
    }

    // Send still exported: a fresh process re-applies the env overlay, so the
    // EFFECTIVE runtime value is again the env one even though the file holds
    // the user's X. This is the pre-existing env semantics, unchanged by c19.
    {
        cc::core::ConfigManager env_reload(project_path.parent_path() / "g.json",
                                           project_path);
        ASSERT_TRUE(env_reload.load().has_value());
        EXPECT_EQ(env_reload.settings().model.default_model, "Y-env-model-c19");
        EXPECT_EQ(env_reload.settings().model.max_output_tokens, 9999u);
    }

    // With the env unset, the file the user wrote is what a fresh process
    // sees — proving the explicit write persisted.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        EnvironmentUnsetGuard unset_tokens("LOOM_MAX_TOKENS");
        cc::core::ConfigManager reloaded(project_path.parent_path() / "g.json",
                                         project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_EQ(reloaded.settings().model.default_model, "X-user-model-c19");
        EXPECT_EQ(reloaded.settings().model.max_output_tokens, 2048u);
    }

    cleanup();
}

// RFC-0001 followup c20 — /mcp xaa show is presence-only: it prints
// "(stored in keychain)" when a secret is stored, never the value itself.
// Drives the REAL command path (AppCommandRegistry) with a secret stored via
// /mcp xaa setup --client-secret, then asserts the show output does not echo
// the secret bytes. The env var is unset before `show` so the assertion
// proves the secret was persisted to the hardened store, not read from the
// environment.
TEST(AppCommandRegistry, XaaShowPresenceOnlyDoesNotEchoSecret) {
    namespace fs = std::filesystem;
    const auto root = cmd_make_temp_root("loom_cmd_c20_xaa_show_");
    const auto home = root / "home";
    const auto cfg  = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(home);
    fs::create_directories(cfg);
    fs::create_directories(work);

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs::path previous_cwd = fs::current_path();
    fs::current_path(work);

    auto cleanup = [&] {
        std::error_code ec;
        fs::current_path(previous_cwd, ec);
        fs::remove_all(root);
    };

    {
        cc::commands::AppCommandRegistry registry;

        // Store a secret the supported way: /mcp xaa setup --client-secret
        // reads MCP_XAA_IDP_CLIENT_SECRET and writes it to the hardened
        // ~/.config/loom/xaa/idp_tokens.json store.
        {
            EnvironmentGuard secret_guard(
                "MCP_XAA_IDP_CLIENT_SECRET", "show-secret-never-echoed");
            auto setup = registry.execute(
                "/mcp xaa setup --issuer https://idp.example.com "
                "--client-id loom-cli --client-secret",
                ctx());
            ASSERT_TRUE(setup.has_value());
            ASSERT_TRUE(setup->ok) << setup->message;
        }
        // The env var is now restored/unset. The secret lives only in the
        // hardened store.

        auto show = registry.execute("/mcp xaa show", ctx());
        ASSERT_TRUE(show.has_value());
        ASSERT_TRUE(show->ok) << show->message;

        // Presence-only: the output says a secret is stored, but never
        // echoes the value.
        EXPECT_NE(show->message.find("(stored in keychain)"), std::string::npos)
            << show->message;
        EXPECT_EQ(show->message.find("show-secret-never-echoed"), std::string::npos)
            << "secret value leaked into /mcp xaa show output: " << show->message;
    }

    cleanup();
}
