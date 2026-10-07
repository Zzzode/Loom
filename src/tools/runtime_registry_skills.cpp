// Implementation unit for loom.tools.runtime_registry — the terminal manual
// SKILL.md fallback, the runtime tool-name list, and tool_search. The B12
// SkillLoader executor slot itself lives in
// loom.tools.runtime_backends.port (anchored runtime_backends_port.cpp).
module;

#include <cstdlib>  // std::getenv("HOME") in execute_skill_tool

module loom.tools.runtime_registry;

import std;

import loom.tools.tool;
import loom.tools.feature_flags;
import loom.serdes.json;

namespace loom::tools::detail {

using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

namespace fs = std::filesystem;

[[nodiscard]] Result<ToolResult> execute_skill_tool(const ToolInput& input) {
    auto name = json_string(input.json(), "name").or_else([&] { return json_string(input.json(), "skill"); });
    if (!name || name->empty()) return ToolResult::error("skill requires name");

    // Terminal fallback for when no orchestration-installed SkillLoader
    // executor claimed the skill (hermetic binaries, unset deployments):
    // walk the well-known skill roots for a <name>/SKILL.md file directly.
    std::vector<fs::path> roots;
    if (const char* home = std::getenv("HOME")) {
        roots.push_back(fs::path{home} / ".codex" / "skills");
        roots.push_back(fs::path{home} / ".agents" / "skills");
    }
    roots.push_back(fs::current_path() / "skills");

    for (const auto& root : roots) {
        auto skill = root / *name / "SKILL.md";
        if (!fs::exists(skill)) continue;
        std::ifstream in(skill);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return ToolResult::success(buffer.str());
    }
    return ToolResult::error(std::format("Skill not found: {}", *name));
}

[[nodiscard]] std::vector<std::string> runtime_tool_names_impl() {
    namespace features = loom::tools::features;

    std::vector<std::string> names = {
        "Agent",
        "computer_use",
        "Edit",
        "Read",
        "WebFetch",
        "WebSearch",
        "Write",
        "ask_user_question",
        "brief",
        "config",
        "enter_plan_mode",
        "exit_plan_mode",
        "list_mcp_resources",
        "mcp",
        "mcp_auth",
        "notebook_edit",
        "read_mcp_resource",
        "send_message",
        "shared",
        "skill",
        "synthetic_output",
        "todo_write",
    };
    if constexpr (features::kBashToolEnabled) {
        names.push_back("Bash");
    }
    if constexpr (!features::kEmbeddedSearchTools) {
        names.push_back("Glob");
        names.push_back("Grep");
    }
    if constexpr (features::kWorktreeMode) {
        names.push_back("enter_worktree");
        names.push_back("exit_worktree");
    }
    if constexpr (features::kEnableLspTool) {
        names.push_back("lsp");
    }
    if constexpr (features::kPowerShellTool) {
        names.push_back("powershell");
    }
    // so it is registered unconditionally.  Runtime behavior is controlled by
    // LOOM_REMOTE_TRIGGER_COMMAND env var.
    // In CPP, "repl" delegates to execute_script() which has a working
    // implementation, so it is registered unconditionally.
    names.push_back("repl");
    // In CPP, schedule_cron has a working implementation (loom.tools.cron),
    // so it is registered unconditionally.
    names.push_back("schedule_cron");
    if constexpr (features::kScriptToolEnabled) {
        names.push_back("script");
    }
    if constexpr (features::kEnableSleepTool) {
        names.push_back("sleep");
    }
    if constexpr (features::kTodoV2) {
        for (const auto* t : {"task_create", "task_get", "task_list", "task_output", "task_stop", "task_update"}) {
            names.push_back(t);
        }
    }
    if constexpr (features::kAgentSwarmsEnabled) {
        names.push_back("team_create");
        names.push_back("team_delete");
    }
    if constexpr (features::kTestingPermissionTool) {
        names.push_back("testing");
    }
    if constexpr (features::kToolSearch) {
        names.push_back("tool_search");
    }
    // In CPP, tungsten has a working implementation (loom.tools.tungsten_tool),
    // so it is registered unconditionally.
    names.push_back("tungsten");
    if constexpr (features::kWebBrowserTool) {
        names.push_back("web_browser");
    }
    if constexpr (features::kWorkflowScripts) {
        names.push_back("workflow");
    }
    // Feature-gated stub tools
    if constexpr (features::kPushNotificationTool) names.push_back("push_notification");
    if constexpr (features::kMonitorTool) names.push_back("monitor");
    if constexpr (features::kSendUserFileTool) names.push_back("send_user_file");
    if constexpr (features::kSubscribePRTool) names.push_back("subscribe_pr");
    if constexpr (features::kOverflowTestTool) names.push_back("overflow_test");
    if constexpr (features::kContextCollapse) names.push_back("ctx_inspect");
    if constexpr (features::kTerminalPanel) names.push_back("terminal_capture");
    if constexpr (features::kHistorySnip) names.push_back("snip");
    if constexpr (features::kUdsInbox) names.push_back("list_peers");
    if constexpr (features::kVerifyPlanExecution) names.push_back("verify_plan_execution");
    return names;
}

[[nodiscard]] Result<ToolResult> execute_tool_search(const ToolInput& input) {
    auto query = json_string(input.json(), "query").value_or("");
    auto names = runtime_tool_names_impl();
    std::string out = "Tools:\n";
    std::size_t count = 0;
    for (const auto& name : names) {
        if (!query.empty() && !name.contains(query)) continue;
        out += "- " + name + "\n";
        ++count;
    }
    if (count == 0) out += "No matching tools.\n";
    return ToolResult::success(out);
}

} // namespace loom::tools::detail

namespace loom::tools {

[[nodiscard]] std::vector<std::string> runtime_tool_names() {
    return detail::runtime_tool_names_impl();
}

} // namespace loom::tools
