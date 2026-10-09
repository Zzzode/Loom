/// @file test_runtime_config.cpp
/// @brief Runtime config tool tests.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <httplib.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

import std;
import loom.tools.bash;
import loom.tools.computer_use;
import loom.tools.powershell;
import loom.tools.runtime_computer_use;
import loom.tools.runtime_shared_utils;
import loom.tools.runtime_team_shared;
import loom.tools.runtime_message_delivery;
import loom.tools.send_message;
import loom.tools.web_fetch;
import loom.tools.web_search;
import loom.tools.web_browser;
import loom.orchestration.tools.mcp;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.runtime_backends;
import loom.tools.runtime_backends.port;  // RFC-0001 B15: set_/clear_skill_loader_executor slot API
import loom.tools.image_codec.port;
import loom.tools.worktree;
import loom.orchestration.agent;
import loom.tools.agent_runtime;
import loom.tools.file_read;
import loom.tools.file_write;
import loom.tools.todo_write;
import loom.tools.notebook;
import loom.tools.registry;
import loom.tools.runtime_registry;
import loom.config.config;
import loom.tools.path_validation;
import loom.tools.bash_security;
import loom.tools.bash_permissions;
import loom.tools.task;
import loom.tools.team;
import loom.tools.team_create;
import loom.tools.team_delete;
import loom.tools.tool;
import loom.serdes.json;
import loom.security.tool_deny_rules;
import loom.query.query_engine;
import loom.teams.swarm.backends;
import loom.teams.swarm.helpers;
import loom.teams.team_helpers;
import loom.hooks.tool_permissions;
import loom.services.api.client;
import loom.services.mcp.types;
import loom.services.mcp.connection_manager;  // RFC-0001 B6: svc_mcp::McpServerSnapshot
import loom.tools.repl;
import loom.tools.skill;
import loom.orchestration.agent.utils;
import loom.tools.destructive_command_warning;

namespace fs = std::filesystem;

// The agent helpers exercised by the existing tests live in
// loom::tools::agent::utils after the agent_tool split; re-expose them through
// the loom::tools::agent namespace so the historical call sites still resolve.
namespace loom::tools::agent { using namespace utils; }

namespace {

// Tests exercise runtime-tool *executor* logic, not permission gating. Supply
// an allow-all live checker so the fail-closed default in RuntimeFunctionTool
// does not block them. Production paths must supply a real checker (or accept
// fail-closed denial for write/execute/network tools).
loom::tools::agent::AgentLivePermissionCheckFn test_allow_all_check() {
    auto allow = []([[maybe_unused]] std::string_view,
                    [[maybe_unused]] std::string_view,
                    [[maybe_unused]] std::string_view) {
        return loom::tools::agent::AgentLivePermissionCheck{.allowed = true};
    };
    return loom::tools::agent::AgentLivePermissionCheckFn{std::move(allow)};
}

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

} // namespace

// ===========================================================================
// RFC-0001 B followup c13b — structured 'config' runtime tool, registry E2E.
// ===========================================================================
namespace {

struct RtConfigJson {
    loom::utils::json::JsonDoc doc;
    loom::utils::json::JsonVal root;
    explicit RtConfigJson(std::string_view text) {
        if (auto parsed = loom::utils::json::parse(text)) {
            doc = std::move(*parsed);
            root = doc.root();
        }
    }
};

// Temp HOME + cwd with LOOM_CONFIG_DIR unset: the default ConfigManager the
// backend builds per call resolves a fully hermetic user file.
struct RtConfigEnv {
    fs::path root;
    fs::path home;
    fs::path work;
    EnvironmentUnsetGuard dir_guard;
    EnvironmentGuard home_guard;
    CurrentPathGuard cwd_guard;

    RtConfigEnv()
        : root(fs::temp_directory_path() /
               ("loom_rt_config_" +
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()))),
          dir_guard("LOOM_CONFIG_DIR"),
          home_guard("HOME", [&] { fs::create_directories(root);
                                   home = root / "home";
                                   work = root / "work";
                                   fs::create_directories(home);
                                   fs::create_directories(work);
                                   return home.string(); }()),
          cwd_guard(work) {}

    ~RtConfigEnv() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    [[nodiscard]] fs::path user_file() const { return home / ".loom" / "settings.json"; }

    static loom::core::ToolRegistry& registry_with_perms(
        loom::core::ToolRegistry& registry) {
        loom::tools::register_runtime_tools(
            registry,
            loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        return registry;
    }
};

[[nodiscard]] loom::core::Result<loom::core::ToolResult>
rt_config_run(loom::core::ToolRegistry& registry, std::string_view json) {
    return registry.execute("config",
                            loom::core::ToolInput::from_json(std::string(json)));
}

} // namespace

// get on a missing installation returns structured defaults and creates no
// files or directories.
TEST(RuntimeConfigTool, GetMissingReturnsDefaultsAndCreatesNothing) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    auto result = rt_config_run(registry, R"({"action":"get"})");
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->content.empty());
    EXPECT_FALSE(result->is_error) << result->content.front().text;

    RtConfigJson parsed(result->content.front().text);
    ASSERT_TRUE(parsed.root.is_obj());
    EXPECT_EQ(std::string(parsed.root.get("action").as_str()), "get");
    EXPECT_EQ(parsed.root.get("user_file_valid").as_bool(), true);
    const auto settings = parsed.root.get("settings");
    EXPECT_TRUE(settings.get("model").get("value").is_str());
    EXPECT_EQ(std::string(settings.get("model").get("source").as_str()),
              "default");
    EXPECT_TRUE(settings.get("temperature").get("value").is_null());

    EXPECT_FALSE(fs::exists(env.home / ".loom"));
    EXPECT_FALSE(fs::exists(env.work / ".loom"));
    EXPECT_FALSE(fs::exists(env.home / ".config"));
}

// Typed native values land on disk and are verified by a real ConfigManager
// load; a single-key get reports source=file.
TEST(RuntimeConfigTool, SetTypedValuesVerifiedByRealLoad) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    const std::array<std::string_view, 4> sets = {{
        R"({"action":"set","key":"temperature","value":0.7})",
        R"({"action":"set","key":"extendedThinking","value":true})",
        R"({"action":"set","key":"maxRetries","value":2})",
        R"({"action":"set","key":"model","value":"rt-model"})",
    }};
    for (const auto payload : sets) {
        auto result = rt_config_run(registry, payload);
        ASSERT_TRUE(result.has_value()) << payload;
        EXPECT_FALSE(result->is_error) << result->content.front().text;
    }

    loom::core::ConfigManager manager;
    ASSERT_TRUE(manager.load(loom::core::LoadOptions{.quiet = true}).has_value());
    EXPECT_DOUBLE_EQ(*manager.settings().model.temperature, 0.7);
    EXPECT_TRUE(manager.settings().model.extended_thinking);
    EXPECT_EQ(manager.settings().network.max_retries, 2u);
    EXPECT_EQ(manager.settings().model.default_model, "rt-model");

    auto one = rt_config_run(
        registry, R"({"action":"get","key":"maxRetries"})");
    ASSERT_TRUE(one.has_value());
    RtConfigJson parsed(one->content.front().text);
    const auto setting = parsed.root.get("setting");
    EXPECT_EQ(std::string(setting.get("source").as_str()), "file");
    EXPECT_EQ(setting.get("value").as_int(), 2);
    EXPECT_EQ(setting.get("writable").as_bool(), true);
}

// Invalid values, read-only keys, blocked surfaces, and unknown keys are
// terminal tool errors.
TEST(RuntimeConfigTool, InvalidReadonlyBlockedUnknownAreErrors) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    auto expect_error = [&](std::string_view payload, std::string_view hint) {
        auto result = rt_config_run(registry, payload);
        ASSERT_TRUE(result.has_value()) << payload;
        EXPECT_TRUE(result->is_error) << payload;
        EXPECT_NE(result->content.front().text.find(hint),
                  std::string::npos)
            << result->content.front().text;
        EXPECT_FALSE(fs::exists(env.user_file()));
    };
    expect_error(R"({"action":"set","key":"temperature","value":2})",
                 "between 0 and 1");
    expect_error(R"({"action":"set","key":"model","value":""})",
                 "must not be empty");
    expect_error(R"({"action":"set","key":"theme","value":"dark"})",
                 "not writable through this tool");
    expect_error(R"({"action":"set","key":"apiKey","value":"abc"})",
                 "LOOM_API_KEY");
    expect_error(R"({"action":"set","key":"mcpServers","value":{}})",
                 "loom mcp");
    expect_error(R"({"action":"set","key":"xaaIdp","value":{}})", "/mcp xaa");
    expect_error(R"({"action":"set","key":"nope.nope","value":1})",
                 "Unknown configuration key");
    expect_error(R"({"action":"set"})", "requires a key");
    expect_error(R"({"action":"set","key":"maxRetries"})",
                 "requires a value");
    expect_error(R"({"action":"get","key":"nope.nope"})",
                 "Unknown configuration key");
}

// $LOOM_CONFIG_DIR routes both the write and subsequent reads.
TEST(RuntimeConfigTool, HonorsConfigDirEnvRouting) {
    const auto root = fs::temp_directory_path() / "loom_rt_config_dir";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto cfg_dir = root / "cfg";
    fs::create_directories(cfg_dir);
    EnvironmentGuard dir_guard("LOOM_CONFIG_DIR", cfg_dir.string());
    CurrentPathGuard cwd_guard(root);

    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);
    auto result = rt_config_run(
        registry, R"({"action":"set","key":"maxRetries","value":6})");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error) << result->content.front().text;
    EXPECT_TRUE(fs::exists(cfg_dir / "settings.json"));

    auto got = rt_config_run(registry, R"({"action":"get"})");
    RtConfigJson parsed(got->content.front().text);
    EXPECT_EQ(parsed.root.get("settings").get("maxRetries")
                  .get("value").as_int(), 6);

    std::error_code ec;
    fs::remove_all(root, ec);
}

// Junk user files are repaired through the salvage state machine and the
// tri-state string is in the response.
TEST(RuntimeConfigTool, JunkRepairResponsesCarrySalvageState) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    fs::create_directories(env.user_file().parent_path());
    {
        std::ofstream(env.user_file())
            << R"({"model":{"default_model":"kept"}} TRAILING JUNK)";
    }
    auto dropped = rt_config_run(
        registry,
        R"({"action":"set","key":"maxRetries","value":3})");
    ASSERT_TRUE(dropped.has_value());
    RtConfigJson d(dropped->content.front().text);
    EXPECT_EQ(std::string(d.root.get("repaired").as_str()),
              "trailing_junk_dropped");

    {
        std::ofstream(env.user_file()) << "[1, 2] AND JUNK";
    }
    auto replaced = rt_config_run(
        registry,
        R"({"action":"set","key":"maxRetries","value":4})");
    ASSERT_TRUE(replaced.has_value());
    RtConfigJson r(replaced->content.front().text);
    EXPECT_EQ(std::string(r.root.get("repaired").as_str()),
              "replaced_unparseable");

    auto clean = rt_config_run(
        registry,
        R"({"action":"set","key":"maxRetries","value":5})");
    ASSERT_TRUE(clean.has_value());
    RtConfigJson c(clean->content.front().text);
    EXPECT_TRUE(c.root.get("repaired").is_null());

    auto got = rt_config_run(registry, R"({"action":"get"})");
    RtConfigJson g(got->content.front().text);
    EXPECT_EQ(g.root.get("settings").get("maxRetries")
                  .get("value").as_int(), 5);
}

// list returns the closed writable/read-only/blocked sets plus null-clear
// metadata.
TEST(RuntimeConfigTool, ListPayloadShape) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    auto result = rt_config_run(registry, R"({"action":"list"})");
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->content.empty());
    RtConfigJson parsed(result->content.front().text);
    ASSERT_TRUE(parsed.root.is_obj());
    const auto specs = parsed.root.get("specs");
    EXPECT_EQ(specs.get("writable").size(), 7u);
    EXPECT_EQ(specs.get("read_only").size(), 9u);
    EXPECT_GE(specs.get("blocked").size(), 9u);
    EXPECT_EQ(specs.get("null_clear_keys").size(), 2u);
    EXPECT_TRUE(specs.get("null_clear_note").is_str());
}

// Null slot fails closed with the standard text; make_config_backend
// reinstalls the real handler.
TEST(RuntimeConfigTool, NullSlotFailsClosedThenReinstalls) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    loom::tools::clear_config_backend();
    {
        auto result = rt_config_run(registry, R"({"action":"get"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->is_error);
        EXPECT_EQ(result->content.front().text,
                  "Runtime tool 'config' has no runtime handler");
    }
    loom::tools::set_config_backend(
        loom::orchestration::make_config_backend());
    {
        auto result = rt_config_run(registry, R"({"action":"get"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->is_error) << result->content.front().text;
    }
}

// Credential bytes never appear in any tool response, from file or env.
TEST(RuntimeConfigTool, SecretBytesNeverLeakResponses) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    constexpr std::string_view kSecret = "SECRET-rt-c13-distinctive-4242";
    fs::create_directories(env.user_file().parent_path());
    {
        std::ofstream f(env.user_file());
        f << "{\"network\":{\"api_key\":\"" << kSecret
          << "\",\"base_url\":\"https://" << kSecret << ".invalid/\"}}";
    }
    EnvironmentGuard key_guard("LOOM_API_KEY", std::string(kSecret));

    const std::array<std::string_view, 3> payloads = {{
        R"({"action":"get","key":"apiKey"})",
        R"({"action":"get","key":"baseUrl"})",
        R"({"action":"get"})",
    }};
    for (const auto payload : payloads) {
        auto result = rt_config_run(registry, payload);
        ASSERT_TRUE(result.has_value()) << payload;
        EXPECT_EQ(result->content.front().text.find(kSecret),
                  std::string::npos)
            << result->content.front().text;
    }

    auto presence = rt_config_run(
        registry, R"({"action":"get","key":"apiKey"})");
    RtConfigJson parsed(presence->content.front().text);
    const auto setting = parsed.root.get("setting");
    EXPECT_EQ(setting.get("set").as_bool(), true);
    EXPECT_EQ(std::string(setting.get("source").as_str()), "env");
}

// LOOM_AUTH_TOKEN (Bearer on the wire) also yields set:true presence
// for network.api_key without leaking the token, alone and alongside
// LOOM_API_KEY.
TEST(RuntimeConfigTool, AuthTokenPresenceAndSecretOmission) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);

    constexpr std::string_view kTokenSecret =
        "SECRET-rt-c13c-auth-token-9911";
    {
        EnvironmentUnsetGuard no_api_key("LOOM_API_KEY");
        EnvironmentGuard token_guard("LOOM_AUTH_TOKEN",
                                     std::string(kTokenSecret));

        auto presence = rt_config_run(
            registry, R"({"action":"get","key":"apiKey"})");
        ASSERT_TRUE(presence.has_value());
        EXPECT_EQ(presence->content.front().text.find(kTokenSecret),
                  std::string::npos)
            << presence->content.front().text;
        RtConfigJson parsed(presence->content.front().text);
        const auto setting = parsed.root.get("setting");
        EXPECT_EQ(setting.get("set").as_bool(), true);
        EXPECT_EQ(std::string(setting.get("source").as_str()), "env");

        // Combined with an API key: still set, and neither secret leaks.
        EnvironmentGuard key_guard("LOOM_API_KEY",
                                   "SECRET-rt-c13c-api-key-3322");
        auto both = rt_config_run(
            registry, R"({"action":"get","key":"apiKey"})");
        ASSERT_TRUE(both.has_value());
        const auto& text = both->content.front().text;
        EXPECT_EQ(text.find("SECRET-rt-c13c-auth-token-9911"),
                  std::string::npos);
        EXPECT_EQ(text.find("SECRET-rt-c13c-api-key-3322"),
                  std::string::npos);
        RtConfigJson bparsed(text);
        EXPECT_EQ(bparsed.root.get("setting").get("set").as_bool(), true);
    }

    // Neither set → presence false / source none.
    EnvironmentUnsetGuard no_api_key("LOOM_API_KEY");
    EnvironmentUnsetGuard no_token("LOOM_AUTH_TOKEN");
    auto none = rt_config_run(
        registry, R"({"action":"get","key":"apiKey"})");
    RtConfigJson nparsed(none->content.front().text);
    EXPECT_EQ(nparsed.root.get("setting").get("set").as_bool(), false);
    EXPECT_EQ(std::string(nparsed.root.get("setting").get("source").as_str()),
              "none");
}

// A set shadowed by an engaged env var still writes and reports shadowed.
TEST(RuntimeConfigTool, EnvShadowDisclosure) {
    RtConfigEnv env;
    loom::core::ToolRegistry registry;
    RtConfigEnv::registry_with_perms(registry);
    EnvironmentGuard model_guard("LOOM_MODEL", "env-rt-model");

    auto result = rt_config_run(
        registry,
        R"({"action":"set","key":"model","value":"file-rt-model"})");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error) << result->content.front().text;
    RtConfigJson parsed(result->content.front().text);
    EXPECT_EQ(parsed.root.get("shadowed").as_bool(), true);
    EXPECT_EQ(std::string(parsed.root.get("shadowed_by").as_str()),
              "LOOM_MODEL");
    EXPECT_EQ(std::string(parsed.root.get("value").as_str()),
              "file-rt-model");

    auto got = rt_config_run(
        registry, R"({"action":"get","key":"model"})");
    RtConfigJson g(got->content.front().text);
    EXPECT_EQ(std::string(g.root.get("setting").get("source").as_str()),
              "env");
    EXPECT_EQ(std::string(g.root.get("setting").get("value").as_str()),
              "env-rt-model");
}
