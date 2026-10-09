/// @file test_powershell.cpp
/// @brief PowerShell tool tests.

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

} // namespace


TEST(Tools, PowerShellToolValidatesCommandAndDangerousCmdlets) {
    auto root = fs::temp_directory_path() / "loom_powershell_validation_test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::tools::PowerShellTool tool(root);
    EXPECT_EQ(tool.check_permission("Get-ChildItem"), loom::tools::CmdletPermission::Allowed);
    EXPECT_EQ(
        tool.check_permission("Remove-Item -Recurse C:\\Temp"),
        loom::tools::CmdletPermission::NeedsApproval
    );

    auto empty = tool.validate(loom::tools::PowerShellConfig{
        .command = "",
        .working_directory = root,
    });
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error(), loom::tools::PowerShellError::CommandEmpty);

    auto missing_cwd = tool.validate(loom::tools::PowerShellConfig{
        .command = "Get-ChildItem",
        .working_directory = root / "missing",
    });
    ASSERT_FALSE(missing_cwd.has_value());
    EXPECT_EQ(missing_cwd.error(), loom::tools::PowerShellError::InvalidWorkingDirectory);

    auto dangerous = tool.validate(loom::tools::PowerShellConfig{
        .command = "Remove-Item -Recurse C:\\Temp",
        .working_directory = root,
    });
    ASSERT_FALSE(dangerous.has_value());
    EXPECT_EQ(dangerous.error(), loom::tools::PowerShellError::DangerousCmdlet);

    auto allowed = tool.validate(loom::tools::PowerShellConfig{
        .command = "Get-ChildItem",
        .working_directory = root,
    });
    EXPECT_TRUE(allowed.has_value());

    fs::remove_all(root);
}

TEST(Tools, PowerShellEncodingHandlerDecodesUtf16LeAndRejectsOddBytes) {
    const std::array<std::byte, 4> utf16_ok{
        std::byte{0x4f}, std::byte{0x00}, std::byte{0x4b}, std::byte{0x00}
    };
    auto decoded = loom::tools::EncodingHandler::utf16le_to_utf8(utf16_ok);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, "OK");

    const std::array<std::byte, 1> odd{std::byte{0x4f}};
    auto invalid = loom::tools::EncodingHandler::utf16le_to_utf8(odd);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), loom::tools::PowerShellError::EncodingError);

    const std::array<std::byte, 2> bom{std::byte{0xff}, std::byte{0xfe}};
    EXPECT_TRUE(loom::tools::EncodingHandler::has_utf16_bom(bom));
}

TEST(Tools, PowerShellEncodedCommandUsesUtf16LeForUtf8Input) {
    EXPECT_EQ(loom::tools::powershell_encoded_command("A"), "QQA=");
    EXPECT_EQ(loom::tools::powershell_encoded_command("\xe4\xbd\xa0"), "YE8=");
    EXPECT_EQ(loom::tools::powershell_single_quote("C:\\It'S\\Here"), "'C:\\It''S\\Here'");
}

TEST(Tools, RuntimePowerShellToolReportsUnavailableOnNonWindows) {
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("powershell", loom::core::ToolInput::from_json(R"({
      "command": "Get-ChildItem"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->content.empty());
#ifdef _WIN32
    GTEST_SKIP() << "PowerShell runtime execution depends on the Windows host shell";
#else
    EXPECT_TRUE(result->is_error);
    EXPECT_NE(result->content.front().text.find("only available on Windows"), std::string::npos);
#endif
}

TEST(Tools, RuntimePowerShellToolValidatesDangerousCommandBeforePlatformExecution) {
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("powershell", loom::core::ToolInput::from_json(R"({
      "command": "Remove-Item -Recurse C:\\Temp"
    })"));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Dangerous cmdlet detected"), std::string::npos);
}

TEST(Tools, RuntimePowerShellToolValidatesWorkingDirectoryBeforePlatformExecution) {
    auto root = fs::temp_directory_path() / "loom_powershell_runtime_cwd_test";
    fs::remove_all(root);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("powershell", loom::core::ToolInput::from_json(std::format(R"({{
      "command": "Get-ChildItem",
      "cwd": "{}"
    }})", (root / "missing").string())));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Working directory does not exist"), std::string::npos);
}

TEST(Tools, RuntimePowerShellToolExecutesRealCommandWithWorkingDirectoryOnWindows) {
#ifndef _WIN32
    GTEST_SKIP() << "Real PowerShell execution is only available on Windows";
#else
    auto root = fs::temp_directory_path() / "loom_powershell_runtime_windows_e2e";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("powershell", loom::core::ToolInput::from_json(std::format(R"({{
      "command": "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; Write-Output (Get-Location).Path",
      "cwd": "{}"
    }})", loom::tools::agent::json_escape_string(root.string()))));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error) << (result->content.empty() ? "" : result->content.front().text);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find(root.string()), std::string::npos);

    fs::remove_all(root);
#endif
}
