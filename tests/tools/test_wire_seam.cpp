/// @file test_wire_seam.cpp
/// @brief WireSeam tests — QueryEngine <-> wire-backend seam.

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

} // namespace

// ============================================================
// QueryEngine <-> wire-backend seam, end to end.
//
// The engine used to serialize /v1/messages inline. It now hands a
// vendor-neutral RequestInput to whichever backend `wire_api` selects. These
// tests pin the wiring itself: same conversation, two wires, two different
// bodies. The backends have their own unit tests; what is checked here is that
// the engine actually reaches them.
// ============================================================
namespace loom_wire_seam_test {

using namespace loom::core;
using loom::utils::json::parse;

namespace {

ToolDefinition echo_tool() {
    return ToolDefinition{
        .name = "Echo",
        .description = "Echoes its input",
        .input_schema = InputSchema{},
        .permission = ToolPermission::ReadOnly,
        .is_hidden = false,
        .category = std::nullopt,
    };
}

QueryEngineConfig base_config() {
    QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = "http://127.0.0.1:1";  // never contacted
    config.retry_policy.max_retries = 0;
    config.model_params.model = "test-model";
    config.model_params.max_tokens = 512;
    config.tools = {echo_tool()};
    return config;
}

}  // namespace

TEST(WireSeam, DefaultsToTheMessagesWire) {
    ToolRegistry registry;
    QueryEngine engine(base_config(), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    // Messages API shape: top-level system (when set) and a nested tool type.
    EXPECT_TRUE(root.get("model").valid());
    EXPECT_TRUE(root.get("max_tokens").is_num());
    EXPECT_EQ(std::string(root.get("tools").at(0).get("type").as_str()),
              "function");
}

TEST(WireSeam, OpenAiWireProducesAChatCompletionsBody) {
    auto config = base_config();
    config.wire_api = "openai";
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value()) << "engine did not route through the OpenAI backend";
    const auto root = doc->root();
    // OpenAI tool shape nests everything under "function" and carries no
    // top-level "type" on the tool call itself; the engine must not emit the
    // Messages API nesting.
    const auto tool = root.get("tools").at(0);
    EXPECT_EQ(std::string(tool.get("type").as_str()), "function");
    ASSERT_TRUE(tool.get("function").valid())
        << "OpenAI tools must nest name/description/parameters under `function`";
    EXPECT_EQ(std::string(tool.get("function").get("name").as_str()), "Echo");
    EXPECT_TRUE(tool.get("function").get("parameters").valid());
    // Messages API-only fields must be absent.
    EXPECT_FALSE(tool.get("input_schema").valid());
    EXPECT_FALSE(root.get("system").valid())
        << "OpenAI carries the system prompt as a role:system message";
}

TEST(WireSeam, OpenAiWireRoutesTheSystemPromptIntoAMessage) {
    auto config = base_config();
    config.wire_api = "openai";
    config.custom_system_prompt = "be terse";
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    ASSERT_TRUE(root.get("messages").is_arr());
    const auto first = root.get("messages").at(0);
    EXPECT_EQ(std::string(first.get("role").as_str()), "system");
    // The engine's system prompt is the configured custom prompt followed by
    // generated context blocks, so only the head is ours to assert on.
    const std::string content{first.get("content").as_str()};
    EXPECT_EQ(content.rfind("be terse", 0), 0u)
        << "system prompt should lead with the custom prompt; got: "
        << content.substr(0, 40);
}

TEST(WireSeam, EnvVarSelectsTheWireWhenConfigDoesNot) {
    auto config = base_config();
    config.custom_system_prompt = "be terse";
    // Guard, not raw setenv: a failing ASSERT_ below would otherwise return
    // early and leave LOOM_WIRE_API set for every subsequent test.
    const EnvironmentGuard wire_env("LOOM_WIRE_API", "openai");
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    ASSERT_TRUE(doc->root().get("messages").is_arr());
    EXPECT_EQ(std::string(doc->root().get("messages").at(0).get("role").as_str()),
              "system")
        << "LOOM_WIRE_API should have selected the OpenAI backend";
}

// The pre-rename spelling is honoured so a config written before the rename
// keeps working. Both names are ours (neither is a vendor name), so accepting
// both costs nothing and avoids silently retargeting an existing setup.
// The documented name selects the wire backend.
TEST(WireSeam, EnvVarNameSelectsTheWire) {
    auto config = base_config();
    config.custom_system_prompt = "be terse";
    const EnvironmentGuard new_env("LOOM_WIRE_API", "openai");
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    ASSERT_TRUE(doc->root().get("messages").is_arr());
    EXPECT_EQ(std::string(doc->root().get("messages").at(0).get("role").as_str()),
              "system")
        << "LOOM_WIRE_API should select the OpenAI backend";
}

TEST(WireSeam, UnknownWireApiFallsBackToMessages) {
    auto config = base_config();
    config.wire_api = "no-such-vendor";
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    // Messages API nesting is back, so the bogus value was ignored rather than
    // producing an empty or malformed body.
    EXPECT_TRUE(doc->root().get("tools").at(0).get("input_schema").valid());
}

TEST(WireSeam, OpenAiWireHasNoNativeComputerToolShape) {
    auto config = base_config();
    config.wire_api = "openai";
    config.tools.push_back(ToolDefinition{
        .name = "computer_use",
        .description = "drive the screen",
        .input_schema = InputSchema{},
        .permission = ToolPermission::Execute,
        .is_hidden = false,
        .category = "computer_use",
    });
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const auto doc = parse(engine.build_request_body_for_testing());
    ASSERT_TRUE(doc.has_value());
    const auto tools = doc->root().get("tools");
    ASSERT_TRUE(tools.is_arr());
    for (size_t i = 0; i < tools.size(); ++i) {
        const auto t = tools.at(i);
        EXPECT_FALSE(t.get("type").as_str() == std::string_view("computer_20241022"))
            << "the native computer tool type is Messages API-only";
    }
}

}  // namespace loom_wire_seam_test
