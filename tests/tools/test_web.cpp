/// @file test_web.cpp
/// @brief Web fetch, web search, and web browser tests.

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


TEST(Tools, WebBrowserToolUsesScreenshotBackend) {
    bool called = false;
    loom::tools::WebBrowserTool tool([&](
        const loom::tools::BrowserRequest& request,
        const loom::tools::PageState& state) -> std::expected<std::string, loom::tools::BrowserError> {
        called = true;
        EXPECT_EQ(request.action, loom::tools::BrowserAction::Screenshot);
        EXPECT_TRUE(state.url().empty());
        return std::string("iVBORw0KGgo=");
    });

    auto result = tool.execute(loom::tools::BrowserRequest{
        .action = loom::tools::BrowserAction::Screenshot,
        .url = "https://example.test",
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(called);
    EXPECT_EQ(result->content, "Captured browser screenshot.");
    ASSERT_TRUE(result->screenshot_base64.has_value());
    EXPECT_EQ(*result->screenshot_base64, "iVBORw0KGgo=");
    EXPECT_EQ(result->media_type, std::optional<std::string>{"image/png"});
}

TEST(Tools, WebBrowserToolRejectsInteractiveActionsWithoutAutomationBackend) {
    loom::tools::WebBrowserTool tool;

    auto click = tool.execute(loom::tools::BrowserRequest{
        .action = loom::tools::BrowserAction::Click,
        .selector = "#submit",
    });
    ASSERT_FALSE(click.has_value());
    EXPECT_EQ(click.error(), loom::tools::BrowserError::BrowserNotAvailable);

    auto fill = tool.execute(loom::tools::BrowserRequest{
        .action = loom::tools::BrowserAction::FillForm,
        .form_fields = {{
            .selector = "#email",
            .value = "ada@example.test",
        }},
    });
    ASSERT_FALSE(fill.has_value());
    EXPECT_EQ(fill.error(), loom::tools::BrowserError::BrowserNotAvailable);
}

TEST(Tools, WebBrowserToolUsesAutomationBackendForClickAndFillForm) {
    std::vector<loom::tools::BrowserRequest> requests;
    loom::tools::WebBrowserTool tool(
        {},
        [&](const loom::tools::BrowserRequest& request,
            const loom::tools::PageState&) -> std::expected<loom::tools::BrowserResult, loom::tools::BrowserError> {
            requests.push_back(request);
            return loom::tools::BrowserResult{
                .content = std::format("automated {}", loom::tools::action_name(request.action)),
            };
        });

    auto click = tool.execute(loom::tools::BrowserRequest{
        .action = loom::tools::BrowserAction::Click,
        .selector = "#submit",
    });
    ASSERT_TRUE(click.has_value());
    EXPECT_EQ(click->content, "automated click");

    auto fill = tool.execute(loom::tools::BrowserRequest{
        .action = loom::tools::BrowserAction::FillForm,
        .form_fields = {{
            .selector = "#email",
            .value = "ada@example.test",
        }},
    });
    ASSERT_TRUE(fill.has_value());
    EXPECT_EQ(fill->content, "automated fill_form");

    ASSERT_EQ(requests.size(), 2u);
    ASSERT_TRUE(requests[0].selector.has_value());
    EXPECT_EQ(*requests[0].selector, "#submit");
    ASSERT_EQ(requests[1].form_fields.size(), 1u);
    EXPECT_EQ(requests[1].form_fields.front().selector, "#email");
    EXPECT_EQ(requests[1].form_fields.front().value, "ada@example.test");
}

TEST(Tools, RuntimeWebBrowserUsesAutomationCommandBackend) {
    EnvironmentGuard automation_guard(
        "LOOM_BROWSER_AUTOMATION_CMD",
        "printf '%s' '{\"content\":\"clicked via command\"}' # {request}"
    );

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("web_browser", loom::core::ToolInput::from_json(R"({
      "action": "click",
      "selector": "#submit"
    })"));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("clicked via command"), std::string::npos);
}

TEST(Tools, RuntimeWebBrowserKeepsPageStateAcrossCalls) {
    EnvironmentUnsetGuard clear_automation_guard("LOOM_BROWSER_AUTOMATION_CMD");
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    {
        EnvironmentGuard automation_guard(
            "LOOM_BROWSER_AUTOMATION_CMD",
            "printf '%s' '{\"content\":\"navigated\",\"title\":\"Runtime Browser State\",\"url\":\"https://example.test\"}' # {request}"
        );
        auto navigate = registry.execute("web_browser", loom::core::ToolInput::from_json(R"({
          "action": "navigate",
          "url": "https://example.test"
        })"));
        ASSERT_TRUE(navigate.has_value());
        ASSERT_FALSE(navigate->is_error);
    }

    auto title = registry.execute("web_browser", loom::core::ToolInput::from_json(R"({
      "action": "get_title"
    })"));
    ASSERT_TRUE(title.has_value());
    EXPECT_FALSE(title->is_error);
    ASSERT_FALSE(title->content.empty());
    EXPECT_EQ(title->content.front().text, "Runtime Browser State");
}

TEST(Tools, WebFetchParsesEscapedUrlFromJson) {
    auto parsed = loom::tools::web_fetch::detail::parse_url(R"({"url":"https://example.com/a?x=\"quoted\"&y=1"})");

    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(*parsed, R"(https://example.com/a?x="quoted"&y=1)");
    EXPECT_FALSE(loom::tools::web_fetch::detail::parse_url(R"({"description":"contains url"})").has_value());
}

TEST(Tools, WebSearchParsesEscapedQueryFromJson) {
    auto parsed = loom::tools::web_search::detail::parse_query(R"({"query":"C++ \"modules\" migration"})");

    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(*parsed, R"(C++ "modules" migration)");
    EXPECT_FALSE(loom::tools::web_search::detail::parse_query(R"({"description":"contains query"})").has_value());
}

TEST(Tools, WebSearchFormatsDuckDuckGoHtmlResults) {
    const std::string html = R"HTML(
      <div class="result">
        <a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.com%2Fdocs%3Fx%3D1%26y%3D2&amp;rut=abc">
          Example &amp; Docs
        </a>
        <a class="result__snippet">Docs <b>about</b> migration &amp; testing.</a>
      </div>
      <div class="result">
        <a class="result__a" href="https://second.example/path">Second Result</a>
      </div>
    )HTML";

    auto formatted = loom::tools::web_search::detail::format_results("migration test", html);

    EXPECT_NE(formatted.find("Search results for: migration test"), std::string::npos);
    EXPECT_NE(formatted.find("1. Example & Docs"), std::string::npos);
    EXPECT_NE(formatted.find("https://example.com/docs?x=1&y=2"), std::string::npos);
    EXPECT_NE(formatted.find("Docs about migration & testing."), std::string::npos);
    EXPECT_NE(formatted.find("2. Second Result"), std::string::npos);
    EXPECT_EQ(formatted.find("result__a"), std::string::npos);
}
