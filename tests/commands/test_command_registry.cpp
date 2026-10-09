/// @file test_command_registry.cpp
/// @brief CommandRegistry and AppCommandRegistry tests.

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

loom::core::CommandContext ctx(std::vector<std::string> args = {}, std::string raw = {}) {
    return loom::core::CommandContext{
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

std::vector<loom::core::Message> compact_runtime_messages(void* state) {
    auto* engine = static_cast<loom::core::QueryEngine*>(state);
    return engine ? engine->get_conversation() : std::vector<loom::core::Message>{};
}

loom::core::VoidResult compact_runtime_apply(void* state) {
    auto* engine = static_cast<loom::core::QueryEngine*>(state);
    if (!engine) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InternalError,
            "No active query engine is available for compaction"));
    }
    auto compacted = engine->compact_conversation();
    if (!compacted) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InternalError,
            compacted.error().format()));
    }
    return loom::core::VoidResult{};
}

} // namespace

TEST(CommandRegistry, ParsesSlashCommandsAndArguments) {
    auto parsed = loom::core::CommandRegistry::parse("/config get model");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->name, "config");
    ASSERT_EQ(parsed->args.size(), 2u);
    EXPECT_EQ(parsed->args[0], "get");
    EXPECT_EQ(parsed->args[1], "model");
    EXPECT_EQ(parsed->raw, "/config get model");

    EXPECT_FALSE(loom::core::CommandRegistry::parse("not a command").has_value());
}

TEST(CommandRegistry, RegistersTypedCommandsAndCompletesNames) {
    loom::core::CommandRegistry registry;
    registry.register_command<loom::commands::HelpCommand>();
    registry.register_command<loom::commands::ClearCommand>();

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
    EXPECT_EQ(loom::commands::command_permission("help"), loom::commands::CommandPermission::ReadOnly);
    EXPECT_EQ(loom::commands::command_permission("clear"), loom::commands::CommandPermission::ReadWrite);
    EXPECT_EQ(loom::commands::command_permission("unknown"), loom::commands::CommandPermission::None);
}

TEST(AppCommandRegistry, DispatchesMigratedRuntimeCommands) {
    loom::commands::AppCommandRegistry registry;

    EXPECT_GT(registry.command_count(), 0u);
    EXPECT_TRUE(registry.has_command("commit"));
    EXPECT_TRUE(registry.has_command("mcp"));
    EXPECT_TRUE(registry.has_command("ant-trace"));
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

    auto exit = registry.execute("/exit", ctx());
    ASSERT_TRUE(exit.has_value());
    EXPECT_EQ(exit->metadata, "EXIT");
}

// /statusline opens the built-in status bar segment toggle dialog via
// the "UI:statusline" metadata tag (replaces the old shell-PS1 printer).
TEST(AppCommandRegistry, StatuslineOpensDialogMetadata) {
    loom::commands::AppCommandRegistry registry;

    auto result = registry.execute("/statusline", ctx());
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ok);
    ASSERT_TRUE(result->metadata.has_value());
    EXPECT_EQ(*result->metadata, "UI:statusline");
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
        loom::commands::AppCommandRegistry registry;

        // stdio scoped add: the scope value must not become a phantom arg.
        auto scoped = registry.execute(
            "/mcp add s1 node serve.js --scope project", ctx());
        ASSERT_TRUE(scoped.has_value());
        ASSERT_TRUE(scoped->ok) << scoped->message;

        auto project = loom::utils::json::parse_file(work / ".loom" / "settings.json");
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
        auto local = loom::utils::json::parse_file(work / ".loom" / "settings.local.json");
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

        auto user = loom::utils::json::parse_file(cfg / "settings.json");
        ASSERT_TRUE(user.has_value());
        EXPECT_TRUE(user->root().get("mcpServers").has("s2"));
        EXPECT_FALSE(project->root().get("mcpServers").has("s2"));
    }

    cleanup();
}

// RFC-0001 B followup c8: ConfigCommand held a default-constructed
// ConfigManager and never called load(), so /config list and /config get
// rendered built-in DEFAULTS instead of the user's files. Seeds the project
// .loom/settings.json and drives the real AppCommandRegistry (see c6 above).
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
            std::ofstream seed(work / ".loom" / "settings.json");
            seed << "{\n"
                    "  \"model\": \"custom-model-x\",\n"
                    "  \"theme\": \"light\",\n"
                    "  \"timeoutSeconds\": 42\n"
                    "}\n";
        }

        // Construct AFTER env/cwd are set: the command's default
        // ConfigManager binds its paths in its constructor.
        loom::commands::AppCommandRegistry registry;

        auto list = registry.execute("/config list", ctx());
        ASSERT_TRUE(list.has_value());
        ASSERT_TRUE(list->ok) << list->message;
        // Seeded values appear...
        EXPECT_NE(list->message.find("custom-model-x"), std::string::npos);
        EXPECT_NE(list->message.find("= light"), std::string::npos);
        EXPECT_NE(list->message.find("= 42s"), std::string::npos);
        // ...and the defaults they replaced do not.
        EXPECT_EQ(list->message.find("test-model"), std::string::npos);
        EXPECT_EQ(list->message.find("= auto"), std::string::npos);
        EXPECT_EQ(list->message.find("= 120s"), std::string::npos);

        auto get_model = registry.execute("/config get model", ctx());
        ASSERT_TRUE(get_model.has_value());
        ASSERT_TRUE(get_model->ok) << get_model->message;
        EXPECT_EQ(get_model->message, "model = custom-model-x");

        auto get_theme = registry.execute("/config get theme", ctx());
        ASSERT_TRUE(get_theme.has_value());
        ASSERT_TRUE(get_theme->ok) << get_theme->message;
        EXPECT_EQ(get_theme->message, "theme = light");

        auto get_timeout = registry.execute("/config get timeoutSeconds", ctx());
        ASSERT_TRUE(get_timeout.has_value());
        ASSERT_TRUE(get_timeout->ok) << get_timeout->message;
        EXPECT_EQ(get_timeout->message, "timeoutSeconds = 42");
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

    const fs::path project_path = work / ".loom" / "settings.json";
    const fs::path user_path = cfg / "settings.json";

    {
        {
            std::ofstream seed(project_path);
            seed << "{\n"
                    "  \"model\": \"custom-model-x\",\n"
                    "  \"theme\": \"dark\",\n"
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

        loom::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set theme light", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;
        EXPECT_EQ(set->message, "Set theme = light");

        auto project = loom::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        const auto root_node = project->root();
        EXPECT_EQ(root_node.get("model").as_str(),
                  std::string_view("custom-model-x"));
        EXPECT_EQ(root_node.get("theme").as_str(),
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

    const fs::path project_path = work / ".loom" / "settings.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n"
                "  \"x-custom\": { \"tool\": \"loom\" },\n"
                "  \"model\": \"seed-model\",\n"
                "  \"x_custom_leaf\": 42,\n"
                "  \"theme\": \"dark\"\n"
                "}\n";
    }

    {
        loom::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set theme light", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;

        auto project = loom::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        const auto root_node = project->root();
        // The unknown top-level key survives with its value intact.
        ASSERT_TRUE(root_node.has("x-custom"));
        EXPECT_EQ(root_node.get("x-custom").get("tool").as_str(),
                  std::string_view("loom"));
        // The known flat key is preserved, and the unknown sibling
        // survives alongside it.
        EXPECT_EQ(root_node.get("model").as_str(),
                  std::string_view("seed-model"));
        ASSERT_TRUE(root_node.has("x_custom_leaf"));
        EXPECT_EQ(root_node.get("x_custom_leaf").as_int(), 42);
        // The intended write landed.
        EXPECT_EQ(root_node.get("theme").as_str(),
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

    const fs::path project_path = work / ".loom" / "settings.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"model\": \"v1\"\n}\n";
    }

    {
        loom::commands::AppCommandRegistry registry;

        auto get1 = registry.execute("/config get model", ctx());
        ASSERT_TRUE(get1.has_value());
        ASSERT_TRUE(get1->ok) << get1->message;
        EXPECT_EQ(get1->message, "model = v1");

        // External edit between commands.
        {
            std::ofstream edit(project_path, std::ios::trunc);
            edit << "{\n  \"model\": \"v2\"\n}\n";
        }

        // Stat-based invalidation: the second command detects the external
        // edit (content hash change) and re-reads the file instead of serving
        // the latched "v1" snapshot.
        auto get2 = registry.execute("/config get model", ctx());
        ASSERT_TRUE(get2.has_value());
        ASSERT_TRUE(get2->ok) << get2->message;
        EXPECT_EQ(get2->message, "model = v2");
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

    const fs::path project_path = work / ".loom" / "settings.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"mcpServers\": { \"alpha\": { \"command\": \"true\" } }\n}\n";
    }

    {
        // Construct AFTER env/cwd are set: the command's default ConfigManager
        // binds its paths in its constructor.
        loom::commands::McpCommand mcp;

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

    const fs::path project_path = work / ".loom" / "settings.json";

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

        loom::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set theme dark", ctx());
        ASSERT_TRUE(set.has_value());
        EXPECT_FALSE(set->ok);
        EXPECT_FALSE(set->message.empty());

        // File bytes untouched, no atomic-write tmp, no new files in .loom.
        EXPECT_EQ(read_file(project_path), bytes_before);
        EXPECT_FALSE(fs::exists(work / ".loom" / "settings.json.tmp"));
        std::error_code ec;
        EXPECT_EQ(std::distance(fs::directory_iterator(work / ".loom", ec),
                                fs::directory_iterator()), 1);
    }

    cleanup();
}

TEST(AppCommandRegistry, RuntimeSurfaceCommandsExecuteLocalLogic) {
    loom::commands::AppCommandRegistry registry;

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
    loom::commands::AgentsCommand agents;

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
    auto help_def = loom::commands::HelpCommand::definition();
    loom::commands::HelpCommand help;
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
    loom::commands::ClearCommand clear;
    bool screen_cleared = false;
    bool conversation_reset = false;
    clear.set_screen_clear_fn([&] { screen_cleared = true; });
    clear.set_conversation_reset_fn([&]() -> loom::core::VoidResult {
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
    loom::core::ToolRegistry tool_registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = std::filesystem::current_path().string();
    loom::core::QueryEngine engine(std::move(config), tool_registry);

    auto make_user = [](std::string text, int index) {
        loom::core::UserMessage msg{};
        msg.id.value = "compact-user-" + std::to_string(index);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 10; ++i) {
        engine.append_message_for_testing(make_user(
            "retain compact command runtime detail " + std::to_string(i) + " " +
            std::string(400, static_cast<char>('a' + (i % 20))),
            i));
    }

    auto before = engine.get_conversation();
    ASSERT_GT(before.size(), 8u);

    loom::commands::AppCommandRegistry registry;
    auto command_ctx = ctx();
    command_ctx.runtime_state = &engine;
    command_ctx.compact_message_provider = compact_runtime_messages;
    command_ctx.compact_applier = compact_runtime_apply;
    auto result = registry.execute("/compact --target 20", command_ctx);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ok);
    EXPECT_EQ(result->status, loom::core::CommandStatus::Succeeded);
    EXPECT_NE(result->message.find("Compaction complete"), std::string::npos);
    EXPECT_EQ(result->message.find("Summarize the following conversation segments"), std::string::npos);

    auto after = engine.get_conversation();
    EXPECT_LT(after.size(), before.size());
    ASSERT_GT(after.size(), 1u);
    bool found_summary_marker = false;
    for (const auto& message : after) {
        const auto* marker = std::get_if<loom::core::UserMessage>(&message);
        if (!marker || marker->content.empty()) continue;
        const auto* text = std::get_if<loom::core::TextBlock>(&marker->content.front());
        if (text && text->text.find("Preserve these details") != std::string::npos) {
            found_summary_marker = true;
            break;
        }
    }
    EXPECT_TRUE(found_summary_marker);
}

TEST(ConfigCommand, ValidatesRequiredArgumentsAndListsConfig) {
    loom::commands::ConfigCommand config;

    EXPECT_TRUE(config.validate(ctx({"list"})).has_value());
    EXPECT_FALSE(config.validate(ctx({"get"})).has_value());
    EXPECT_FALSE(config.validate(ctx({"set", "model"})).has_value());

    auto list = config.execute(ctx({"list"}));
    ASSERT_TRUE(list.has_value());
    EXPECT_NE(list->message.find("Current Configuration"), std::string::npos);

    auto completions = config.complete("pa");
    ASSERT_FALSE(completions.empty());
    EXPECT_EQ(completions.front(), "path");
}

TEST(ModelCommand, ListsSwitchesAndCompletesModels) {
    loom::commands::ModelCommand model;

    auto list = model.execute(ctx({"list"}));
    ASSERT_TRUE(list.has_value());
    EXPECT_NE(list->message.find("Available models"), std::string::npos);

    auto switched = model.execute(ctx({"set", "test-model"}));
    ASSERT_TRUE(switched.has_value());
    EXPECT_NE(switched->message.find("test-model"), std::string::npos);

    // Any non-empty model ID is valid (no built-in model list)
    auto valid = model.validate(ctx({"not-a-model"}));
    EXPECT_TRUE(valid.has_value());

    // No built-in model completions — only subcommands match
    auto completions = model.complete("claude-");
    EXPECT_TRUE(completions.empty());
}

TEST(MigratedCommandMetadata, HooksAndRewindExposeTypeScriptCompatibleMetadata) {
    auto hooks_def = loom::commands::HooksCommand::definition();
    auto rewind_def = loom::commands::RewindCommand::definition();

    EXPECT_EQ(hooks_def.name, "hooks");
    EXPECT_EQ(hooks_def.description, "View hook configurations for tool events");
    EXPECT_FALSE(hooks_def.hidden);

    ASSERT_EQ(rewind_def.aliases.size(), 1u);
    EXPECT_EQ(rewind_def.aliases.front(), "checkpoint");
}

TEST(MigratedCommands, ExecuteActionableMessagesAndCompletions) {
    loom::commands::HooksCommand hooks;
    auto hooks_result = hooks.execute(ctx({"list"}));
    ASSERT_TRUE(hooks_result.has_value());
    EXPECT_NE(hooks_result->message.find("PreToolUse"), std::string::npos);

    loom::commands::RewindCommand rewind;
    auto rewind_result = rewind.execute(ctx({"code"}));
    ASSERT_TRUE(rewind_result.has_value());
    EXPECT_NE(rewind_result->message.find("code"), std::string::npos);

    auto rewind_completions = rewind.complete("c");
    ASSERT_EQ(rewind_completions.size(), 2u);
    EXPECT_EQ(rewind_completions.front(), "conversation");
}

// RFC-0001 B followup c17a — END-TO-END: `/mcp xaa setup --callback-port`
// persists settings.xaaIdp.callbackPort (settings.json), and the SAME store is
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
        loom::commands::AppCommandRegistry registry;

        auto setup = registry.execute(
            "/mcp xaa setup --issuer https://idp.example.com "
            "--client-id loom-cli --callback-port 19485",
            ctx());
        ASSERT_TRUE(setup.has_value());
        ASSERT_TRUE(setup->ok) << setup->message;

        // The value landed in the SUPPORTED store (settings.json / xaaIdp), not
        // in a separate file. Default save target is the project tier.
        auto project = loom::utils::json::parse_file(work / ".loom" / "settings.json");
        ASSERT_TRUE(project.has_value());
        const auto xaa = project->root().get("xaaIdp");
        ASSERT_TRUE(xaa.is_obj());
        EXPECT_EQ(xaa.get("issuer").as_str(), std::string_view("https://idp.example.com"));
        EXPECT_EQ(xaa.get("clientId").as_str(), std::string_view("loom-cli"));
        ASSERT_TRUE(xaa.get("callbackPort").is_num());
        EXPECT_EQ(xaa.get("callbackPort").as_int(), 19485);

        // The port reaches the login seam that acquire_idp_id_token() binds
        // from: the resolved port is the configured one, not a random port.
        auto opts = loom::services::mcp::build_login_options(
            "https://idp.example.com", "loom-cli",
            std::optional<int>{static_cast<int>(xaa.get("callbackPort").as_int())});
        auto resolved = loom::services::mcp::resolve_login_callback_port(opts);
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

    const fs::path project_path = work / ".loom" / "settings.json";

    {
        {
            std::ofstream seed(project_path);
            seed << "{\n"
                    "  \"model\": \"file-model-c19\",\n"
                    "  \"theme\": \"light\"\n"
                    "}\n";
        }

        // Construct AFTER env + cwd: the command's manager binds paths in its
        // ctor and reads the env on load().
        loom::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set theme dark", ctx());
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
        auto project = loom::utils::json::parse_file(project_path);
        ASSERT_TRUE(project.has_value());
        EXPECT_EQ(project->root().get("model").as_str(),
                  std::string_view("file-model-c19"));
    }

    // The real proof it was not baked: with the env UNSET, a fresh load
    // yields the file's own model values, not the ephemeral ones.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        EnvironmentUnsetGuard unset_tokens("LOOM_MAX_TOKENS");
        loom::core::ConfigManager reloaded(project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_EQ(reloaded.settings().model.default_model, "file-model-c19");
        EXPECT_NE(reloaded.settings().model.max_output_tokens, 4321u);
    }

    cleanup();
}

// RFC-0001 B followup c19: pin the serializer's EXISTING exclusion of the
// network secret/endpoint values at the byte level, so a future serializer
// change cannot silently start leaking an exported LOOM_API_KEY /
// LOOM_BASE_URL / proxy into the tracked project file. Test 2.
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
    EnvironmentGuard api_guard("LOOM_API_KEY", "SECRET-c19-traceable-key");
    EnvironmentGuard base_guard("LOOM_BASE_URL",
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

    const fs::path project_path = work / ".loom" / "settings.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"theme\": \"light\"\n}\n";
    }

    {
        loom::commands::AppCommandRegistry registry;
        auto set = registry.execute("/config set theme dark", ctx());
        ASSERT_TRUE(set.has_value());
        ASSERT_TRUE(set->ok) << set->message;

        const std::string bytes = cmd_read_file(project_path);
        // No transport of the exported credential/endpoint bytes anywhere.
        EXPECT_EQ(bytes.find("SECRET-c19"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("apiKey"), std::string::npos) << bytes;
        EXPECT_EQ(bytes.find("baseUrl"), std::string::npos) << bytes;
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

    const fs::path project_path = work / ".loom" / "settings.json";
    {
        std::ofstream seed(project_path);
        seed << "{\n  \"model\": \"file-seed-c19\"\n}\n";
    }

    {
        loom::commands::AppCommandRegistry registry;

        // Explicit user intent while LOOM_MODEL=Y: X wins on disk.
        auto set_model = registry.execute(
            "/config set model X-user-model-c19", ctx());
        ASSERT_TRUE(set_model.has_value());
        ASSERT_TRUE(set_model->ok) << set_model->message;
        {
            const std::string bytes = cmd_read_file(project_path);
            EXPECT_NE(bytes.find("X-user-model-c19"), std::string::npos) << bytes;
            EXPECT_EQ(bytes.find("Y-env-model-c19"), std::string::npos) << bytes;
            auto project = loom::utils::json::parse_file(project_path);
            ASSERT_TRUE(project.has_value());
            EXPECT_EQ(project->root().get("model").as_str(),
                      std::string_view("X-user-model-c19"));
        }

        // Same rule for maxOutputTokens with LOOM_MAX_TOKENS engaged.
        auto set_tokens = registry.execute(
            "/config set maxOutputTokens 2048", ctx());
        ASSERT_TRUE(set_tokens.has_value());
        ASSERT_TRUE(set_tokens->ok) << set_tokens->message;
        {
            const std::string bytes = cmd_read_file(project_path);
            EXPECT_NE(bytes.find("\"maxOutputTokens\": 2048"), std::string::npos)
                << bytes;
            EXPECT_EQ(bytes.find("9999"), std::string::npos) << bytes;
        }

        // Documented runtime behavior: the env overlay applies at LOAD time,
        // so within THIS manager the explicit set is what /config get reports
        // (the in-memory value is the user's X). A FRESH load while the env
        // is still exported re-applies the overlay and yields Y — see below.
        auto get_model = registry.execute("/config get model", ctx());
        ASSERT_TRUE(get_model.has_value());
        EXPECT_EQ(get_model->message, "model = X-user-model-c19");
    }

    // Send still exported: a fresh process re-applies the env overlay, so the
    // EFFECTIVE runtime value is again the env one even though the file holds
    // the user's X. This is the pre-existing env semantics, unchanged by c19.
    {
        loom::core::ConfigManager env_reload(project_path);
        ASSERT_TRUE(env_reload.load().has_value());
        EXPECT_EQ(env_reload.settings().model.default_model, "Y-env-model-c19");
        EXPECT_EQ(env_reload.settings().model.max_output_tokens, 9999u);
    }

    // With the env unset, the file the user wrote is what a fresh process
    // sees — proving the explicit write persisted.
    {
        EnvironmentUnsetGuard unset_model("LOOM_MODEL");
        EnvironmentUnsetGuard unset_tokens("LOOM_MAX_TOKENS");
        loom::core::ConfigManager reloaded(project_path);
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
        loom::commands::AppCommandRegistry registry;

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
