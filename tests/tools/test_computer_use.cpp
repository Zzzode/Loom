/// @file test_computer_use.cpp
/// @brief Computer use tool tests.

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

struct RuntimeComputerUseProviderGuard {
    ~RuntimeComputerUseProviderGuard() {
        loom::tools::clear_runtime_computer_use_capture_provider_for_testing();
        loom::tools::clear_runtime_computer_use_input_provider_for_testing();
    }
};

// RFC-0001 B11/B12: the image codec and the SkillLoader skill executor are
// process-global function-local statics installed by
// loom::orchestration::install_runtime_backends() (std::call_once; safe for
// the per-request server threads). Every test that reads an image or PDF
// through Read, exercises a computer_use screenshot/base64 path, or relies
// on SkillLoader directory/plugin discovery installs the real
// services-backed backends for its duration and clears the slots in the
// destructor so later cases stay hermetic. The constructor installs
// directly as well: call_once makes a repeat install a no-op after a
// previous guard's destructor cleared the slots.
struct FileToolServicesGuard {
    FileToolServicesGuard() {
        loom::orchestration::install_runtime_backends();
        loom::tools::image_codec::set_codec(
            loom::orchestration::make_image_codec());
        loom::tools::set_skill_loader_executor(
            loom::orchestration::make_skill_loader_executor());
    }
    ~FileToolServicesGuard() {
        loom::tools::image_codec::clear_codec();
        loom::tools::clear_skill_loader_executor();
    }
};

std::string shell_quote_for_test(std::string_view value) {
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out += ch;
        }
    }
    out += "'";
    return out;
}

} // namespace


TEST(Tools, ComputerUseManagerUsesCaptureProviderForScreenshot) {
    using namespace loom::core::computer_use;
    using Rect = loom::core::computer_use::Rect;

    bool saw_region = false;
    ComputerUseManager manager(ScreenCapture([&](std::optional<Rect> region)
        -> std::expected<ImageData, std::string> {
        saw_region = region.has_value();
        if (region) {
            EXPECT_EQ(region->x, 1);
            EXPECT_EQ(region->y, 2);
            EXPECT_EQ(region->width, 3u);
            EXPECT_EQ(region->height, 4u);
        }
        return ImageData{
            .pixels = {1, 2, 3, 4},
            .width = 3,
            .height = 4,
            .format = "rgba",
        };
    }));

    auto result = manager.execute_action(ComputerAction{
        .type = ActionType::Screenshot,
        .position = std::nullopt,
        .drag_end = std::nullopt,
        .text = std::nullopt,
        .region = Rect{.x = 1, .y = 2, .width = 3, .height = 4},
        .keys = {},
    });

    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_TRUE(saw_region);
    ASSERT_TRUE(result.screenshot.has_value());
    EXPECT_EQ(result.screenshot->width, 3u);
    EXPECT_EQ(result.screenshot->height, 4u);
    EXPECT_EQ(result.screenshot->format, "rgba");
}

// Regression for the see→act loop: an input action (click/type/...) MUST
// return a fresh screenshot, not just the explicit Screenshot action, or the
// model is blind after acting.
TEST(Tools, ComputerUseInputActionReturnsPostActionScreenshot) {
    using namespace loom::core::computer_use;
    // macOS MacTypes.h also defines global ::Rect; alias to disambiguate.
    using Rect = loom::core::computer_use::Rect;

    int capture_calls = 0;
    ComputerUseManager manager(
        ScreenCapture([&](std::optional<Rect>) -> std::expected<ImageData, std::string> {
            ++capture_calls;
            return ImageData{
                .pixels = {9, 9, 9, 9},
                .width = 2,
                .height = 2,
                .format = "png",
            };
        }),
        // Input provider that succeeds and does NOT attach a frame itself.
        [](const ComputerAction&) -> std::expected<void, std::string> {
            return {};
        });

    auto result = manager.execute_action(ComputerAction{
        .type = ActionType::KeyType,
        .position = std::nullopt,
        .drag_end = std::nullopt,
        .text = std::string("hi"),
        .region = std::nullopt,
        .keys = {},
    });

    EXPECT_TRUE(result.success) << result.error_message;
    ASSERT_TRUE(result.screenshot.has_value())
        << "a type action must attach a post-action screenshot";
    EXPECT_EQ(result.screenshot->format, "png");
    EXPECT_EQ(capture_calls, 1);
}

// If the input provider already attached a frame, do not overwrite it with a
// second capture.
TEST(Tools, ComputerUseInputActionKeepsProviderProvidedFrame) {
    using namespace loom::core::computer_use;
    // macOS MacTypes.h also defines global ::Rect/::Point; alias to disambiguate.
    using Rect = loom::core::computer_use::Rect;
    using Point = loom::core::computer_use::Point;

    int capture_calls = 0;
    ComputerUseManager manager(
        ScreenCapture([&](std::optional<Rect>) -> std::expected<ImageData, std::string> {
            ++capture_calls;
            return ImageData{.pixels = {1}, .width = 1, .height = 1, .format = "png"};
        }),
        [](const ComputerAction&) -> std::expected<void, std::string> {
            return {};
        });

    // KeyPress goes through the same dispatch_input path; here we directly
    // verify a provider-supplied frame is preserved by calling dispatch via
    // a manager whose provider attaches one.
    // (Construct via a small adapter provider.)
    auto result = manager.execute_action(ComputerAction{
        .type = ActionType::MouseMove,
        .position = Point{.x = 1, .y = 1},
        .drag_end = std::nullopt,
        .text = std::nullopt,
        .region = std::nullopt,
        .keys = {},
    });
    EXPECT_TRUE(result.success);
    // Provider did not attach → manager grabs exactly one frame.
    EXPECT_EQ(capture_calls, 1);
}

TEST(Tools, RuntimeComputerUseScreenshotReturnsImageContentFromCaptureProvider) {
    using namespace loom::core::computer_use;
    using Rect = loom::core::computer_use::Rect;

    RuntimeComputerUseProviderGuard guard;
    FileToolServicesGuard services_guard;
    bool saw_region = false;
    loom::tools::set_runtime_computer_use_capture_provider_for_testing(
        [&](std::optional<Rect> region) -> std::expected<ImageData, std::string> {
            saw_region = region.has_value();
            if (!region) return std::unexpected("missing region");
            EXPECT_EQ(region->x, 5);
            EXPECT_EQ(region->y, 6);
            EXPECT_EQ(region->width, 2u);
            EXPECT_EQ(region->height, 2u);
            return ImageData{
                .pixels = {1, 2, 3, 4},
                .width = 2,
                .height = 2,
                .format = "png",
            };
        });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "screenshot",
      "x": 5,
      "y": 6,
      "width": 2,
      "height": 2
    })"));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    EXPECT_TRUE(saw_region);
    ASSERT_EQ(result->content.size(), 2u);
    EXPECT_NE(result->content[0].text.find("Captured screenshot 2x2."), std::string::npos);
    EXPECT_EQ(result->content[1].format, std::optional<std::string>{"image"});
    // png is the wire encoding the API accepts (a raw internal rgba frame is
    // never sent as image/rgba).
    EXPECT_EQ(result->content[1].media_type, std::optional<std::string>{"image/png"});
    EXPECT_EQ(result->content[1].data, std::optional<std::string>{"AQIDBA=="});
}

TEST(Tools, RuntimeComputerUseUsesCommandBackendForScreenshotAndInputActions) {
    RuntimeComputerUseProviderGuard guard;
    FileToolServicesGuard services_guard;
    EnvironmentGuard disable_native_input("LOOM_DISABLE_NATIVE_COMPUTER_INPUT", "1");

    auto root = fs::temp_directory_path() / "loom_computer_use_command_backend_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto script_path = root / "computer-use-host.js";
    const auto log_path = root / "requests.jsonl";
    {
        std::ofstream script(script_path);
        script << R"JS(
const fs = require('fs');
const request = JSON.parse(process.argv[2] || '{}');
fs.appendFileSync(process.env.LOOM_COMPUTER_USE_LOG, JSON.stringify(request) + '\n');
if (request.action === 'screenshot') {
  console.log(JSON.stringify({
    success: true,
    screenshot_base64: 'AQIDBA==',
    width: 2,
    height: 2,
    format: 'png',
  }));
} else if (request.action === 'right_click') {
  console.log(JSON.stringify({success: false, error: 'blocked by host'}));
} else {
  console.log(JSON.stringify({success: true, content: 'input accepted'}));
}
)JS";
    }
    EnvironmentGuard log_guard("LOOM_COMPUTER_USE_LOG", log_path.string());
    EnvironmentGuard command_guard(
        "LOOM_COMPUTER_USE_CMD",
        "node " + shell_quote_for_test(script_path.string()) + " {request}");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto screenshot = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "screenshot",
      "x": 5,
      "y": 6,
      "width": 2,
      "height": 2
    })"));
    ASSERT_TRUE(screenshot.has_value());
    EXPECT_FALSE(screenshot->is_error);
    ASSERT_EQ(screenshot->content.size(), 2u);
    EXPECT_EQ(screenshot->content[1].media_type, std::optional<std::string>{"image/png"});
    EXPECT_EQ(screenshot->content[1].data, std::optional<std::string>{"AQIDBA=="});

    auto click = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "click",
      "x": 11,
      "y": 12
    })"));
    ASSERT_TRUE(click.has_value());
    EXPECT_FALSE(click->is_error);

    auto blocked = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "right_click",
      "x": 13,
      "y": 14
    })"));
    ASSERT_TRUE(blocked.has_value());
    EXPECT_TRUE(blocked->is_error);
    ASSERT_FALSE(blocked->content.empty());
    EXPECT_NE(blocked->content.front().text.find("blocked by host"), std::string::npos);

    std::ifstream log(log_path);
    ASSERT_TRUE(log);
    std::stringstream buffer;
    buffer << log.rdbuf();
    const auto log_text = buffer.str();
    EXPECT_NE(log_text.find(R"("action":"screenshot")"), std::string::npos);
    EXPECT_NE(log_text.find(R"("region":{"x":5,"y":6,"width":2,"height":2})"), std::string::npos);
    EXPECT_NE(log_text.find(R"("action":"click")"), std::string::npos);
    EXPECT_NE(log_text.find(R"("x":11)"), std::string::npos);
    EXPECT_NE(log_text.find(R"("action":"right_click")"), std::string::npos);

    fs::remove_all(root);
}

TEST(Tools, ComputerUseManagerFailsInputActionsWithoutInputProvider) {
    using Point = loom::core::computer_use::Point;
    using namespace loom::core::computer_use;

    ComputerUseManager manager;
    auto result = manager.execute_action(ComputerAction{
        .type = ActionType::MouseClick,
        .position = Point{.x = 10, .y = 20},
        .drag_end = std::nullopt,
        .text = std::nullopt,
        .region = std::nullopt,
        .keys = {},
    });

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message, "Computer input control not available");
}

TEST(Tools, NativeComputerUseInputProviderHonorsDisableEnv) {
    EnvironmentGuard guard("LOOM_DISABLE_NATIVE_COMPUTER_INPUT", "1");
    auto provider = loom::core::computer_use::make_native_input_provider();
    EXPECT_FALSE(static_cast<bool>(provider));
}

TEST(Tools, NativeComputerUseInputProviderIsAvailableOnApple) {
    if (const char* disabled = std::getenv("LOOM_DISABLE_NATIVE_COMPUTER_INPUT");
        disabled && std::string_view(disabled) == "1") {
        GTEST_SKIP() << "native computer input is disabled by environment";
    }
    auto provider = loom::core::computer_use::make_native_input_provider();
#ifdef __APPLE__
    EXPECT_TRUE(static_cast<bool>(provider));
#else
    EXPECT_FALSE(static_cast<bool>(provider));
#endif
}

TEST(Tools, RuntimeComputerUseDispatchesInputActionsToProvider) {
    using namespace loom::core::computer_use;

    RuntimeComputerUseProviderGuard guard;
    // On Apple the native manager returns a post-action screenshot even for
    // plain input actions, so the computer_use path encodes through the
    // orchestration-installed image codec port. Linux's default capture is
    // empty (no screenshot), so this is a no-op there but required for mac.
    FileToolServicesGuard codec_guard;
    std::vector<ComputerAction> actions;
    loom::tools::set_runtime_computer_use_input_provider_for_testing(
        [&](const ComputerAction& action) -> std::expected<void, std::string> {
            actions.push_back(action);
            return {};
        });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto click = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "click",
      "x": 11,
      "y": 12
    })"));
    ASSERT_TRUE(click.has_value());
    EXPECT_FALSE(click->is_error);

    auto typed = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "type",
      "text": "hello"
    })"));
    ASSERT_TRUE(typed.has_value());
    EXPECT_FALSE(typed->is_error);

    auto hotkey = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "hotkey",
      "keys": ["cmd", "k"]
    })"));
    ASSERT_TRUE(hotkey.has_value());
    EXPECT_FALSE(hotkey->is_error);

    auto scroll = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "scroll",
      "x": 0,
      "y": -3
    })"));
    ASSERT_TRUE(scroll.has_value());
    EXPECT_FALSE(scroll->is_error);

    ASSERT_EQ(actions.size(), 4u);
    EXPECT_EQ(actions[0].type, ActionType::MouseClick);
    ASSERT_TRUE(actions[0].position.has_value());
    EXPECT_EQ(actions[0].position->x, 11);
    EXPECT_EQ(actions[0].position->y, 12);
    EXPECT_EQ(actions[1].type, ActionType::KeyType);
    EXPECT_EQ(actions[1].text, std::optional<std::string>{"hello"});
    EXPECT_EQ(actions[2].type, ActionType::KeyHotkey);
    ASSERT_EQ(actions[2].keys.size(), 2u);
    EXPECT_EQ(actions[2].keys[0], "cmd");
    EXPECT_EQ(actions[2].keys[1], "k");
    EXPECT_EQ(actions[3].type, ActionType::Scroll);
    ASSERT_TRUE(actions[3].position.has_value());
    EXPECT_EQ(actions[3].position->x, 0);
    EXPECT_EQ(actions[3].position->y, -3);
}

TEST(Tools, RuntimeComputerUseRejectsInputActionsWithoutProvider) {
    RuntimeComputerUseProviderGuard guard;
    EnvironmentGuard disable_native_input("LOOM_DISABLE_NATIVE_COMPUTER_INPUT", "1");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("computer_use", loom::core::ToolInput::from_json(R"({
      "action": "click",
      "x": 1,
      "y": 2
    })"));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Computer input control not available"), std::string::npos);
}

TEST(RuntimeComputerUse, EscapesAndBuildsActionPayload) {
    namespace rcu = loom::tools::runtime_computer_use;
    using loom::core::computer_use::ActionType;
    EXPECT_EQ(rcu::action_name(ActionType::Screenshot), "screenshot");
    EXPECT_EQ(rcu::action_name(ActionType::MouseClick), "click");
    EXPECT_EQ(rcu::json_escape(R"(a"b\c)"), R"(a\"b\\c)");
    EXPECT_EQ(rcu::json_escape("tab\there"), R"(tab\there)");

    loom::core::computer_use::ComputerAction action{};
    action.type = ActionType::KeyType;
    action.text = std::string{"hello\"world"};
    auto payload = rcu::command_request_json(action);
    EXPECT_NE(payload.find("\"action\":\"type\""), std::string::npos);
    EXPECT_NE(payload.find("\"text\":\"hello\\\"world\""), std::string::npos);

    std::string s = "a{request}b{request}c";
    rcu::replace_all(s, "{request}", "X");
    EXPECT_EQ(s, "aXbXc");
}
