module;

#include <cstddef>

export module loom.orchestration.agent.spawn_multi_agent;

import std;

import loom.orchestration.agent;
import loom.tools.agent_types;
import loom.types.tool_types;
import loom.tools.tool;

export namespace loom::tools {

namespace detail {
[[nodiscard]] inline std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\':
            out += R"(\\)";
            break;
        case '"':
            out += R"(\")";
            break;
        case '\n':
            out += R"(\n)";
            break;
        case '\r':
            out += R"(\r)";
            break;
        case '\t':
            out += R"(\t)";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

[[nodiscard]] inline std::string agent_type_to_subagent_type(AgentType type,
                                                             std::string_view fallback_name = {}) {
    switch (type) {
    case AgentType::Explore:
        return "Explore";
    case AgentType::Plan:
        return "Plan";
    case AgentType::Verify:
        return "verification";
    case AgentType::GeneralPurpose:
        return "general-purpose";
    case AgentType::Custom:
        return fallback_name.empty() ? std::string{"general-purpose"} : std::string{fallback_name};
    }
    return "general-purpose";
}

[[nodiscard]] inline std::string
agent_prompt(const MultiAgentAgentConfig& agent_config,
             const std::optional<std::string>& coordinator_prompt) {
    if (coordinator_prompt && !coordinator_prompt->empty()) return *coordinator_prompt;
    return "Run the assigned multi-agent task as '" + agent_config.name + "'.";
}

[[nodiscard]] inline std::string tool_result_text(const loom::core::ToolResult& result) {
    std::string text;
    for (const auto& block : result.content) {
        if (!text.empty()) text += "\n";
        text += block.text;
    }
    return text;
}

[[nodiscard]] inline auto failed_agent_result(std::string message) -> MultiAgentResult {
    return MultiAgentResult{
        .output = std::move(message), .turns_used = 0, .tokens_used = 0, .completed = false};
}

[[nodiscard]] inline auto
execute_agent_tool(const MultiAgentAgentConfig& agent_config, std::string prompt,
                   std::string working_dir,
                   const std::shared_ptr<loom::core::ToolRegistry>& registry,
                   std::shared_ptr<const loom::core::ApiExecutionContext> api_context,
                   std::optional<std::string> parent_model,
                   AgentLivePermissionCheckFn permission_check) -> MultiAgentResult {
    if (!registry) return failed_agent_result("Non-team agents require a tool registry.");
    loom::tools::agent::AgentConfig runtime_config{};
    runtime_config.max_turns = agent_config.max_turns.value_or(runtime_config.max_turns);
    if (!agent_config.model.empty()) runtime_config.default_model = agent_config.model;
    runtime_config.allowed_tools = agent_config.allowed_tools;
    runtime_config.api_context = api_context;
    const auto subagent_type = agent_type_to_subagent_type(agent_config.type, agent_config.name);
    if (!runtime_config.allowed_tools.empty()) {
        runtime_config.allowed_tools.push_back("Agent(" + subagent_type + ")");
    }

    loom::tools::agent::AgentTool tool(runtime_config, 0, registry.get(),
                                       std::move(permission_check), true);
    std::ostringstream input;
    input << R"({"prompt":")" << json_escape(prompt) << R"(","subagent_type":")"
          << json_escape(subagent_type) << R"(","run_in_background":false)";
    if (!agent_config.system_prompt.empty()) {
        input << R"(,"override_system_prompt":")" << json_escape(agent_config.system_prompt) << '"';
    }
    if (!agent_config.name.empty()) {
        input << R"(,"name":")" << json_escape(agent_config.name) << '"';
    }
    if (!agent_config.model.empty()) {
        input << R"(,"model":")" << json_escape(agent_config.model) << '"';
    }
    if (!working_dir.empty()) {
        input << R"(,"cwd":")" << json_escape(working_dir) << '"';
    }
    input << '}';

    auto tool_input = loom::core::ToolInput::from_json(input.str());
    tool_input.api_context = std::move(api_context);
    tool_input.parent_model = std::move(parent_model);
    auto result = tool.execute(tool_input);
    if (!result) {
        auto error = result.error().format();
        return failed_agent_result(std::move(error));
    }
    auto output = tool_result_text(*result);
    return MultiAgentResult{
        .output = std::move(output),
        .turns_used = tool.last_run_turns(),
        .tokens_used = tool.last_run_tokens(),
        .completed = !result->is_error,
        .started = tool.last_run_turns() > 0,
    };
}

[[nodiscard]] inline std::string resolve_working_dir(std::string configured) {
    if (!configured.empty()) return configured;
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) return cwd.string();
    return ".";
}

[[nodiscard]] inline std::string
teammate_prompt(const MultiAgentAgentConfig& agent_config,
                const std::optional<std::string>& coordinator_prompt) {
    if (coordinator_prompt && !coordinator_prompt->empty()) return *coordinator_prompt;
    return "You have been spawned as teammate '" + agent_config.name + "'.";
}

[[nodiscard]] inline auto
spawn_teammate_agent(const MultiAgentAgentConfig& agent_config, std::string_view team_name,
                     std::string prompt, std::string working_dir, bool prefer_in_process,
                     const std::shared_ptr<loom::core::ToolRegistry>& registry,
                     std::shared_ptr<const loom::core::ApiExecutionContext> api_context,
                     std::optional<std::string> parent_model,
                     AgentLivePermissionCheckFn permission_check) -> MultiAgentResult {
    if (agent_config.name.empty()) {
        return failed_agent_result("Agent name is required for teammate spawn.");
    }
    if (team_name.empty()) {
        return failed_agent_result("Team name is required for teammate spawn.");
    }

    loom::tools::agent::AgentConfig runtime_config{};
    runtime_config.max_turns = agent_config.max_turns.value_or(runtime_config.max_turns);
    if (!agent_config.model.empty()) runtime_config.default_model = agent_config.model;
    runtime_config.allowed_tools = agent_config.allowed_tools;
    runtime_config.api_context = api_context;
    runtime_config.prefer_in_process_teammate = prefer_in_process;
    const auto subagent_type = agent_type_to_subagent_type(agent_config.type, agent_config.name);
    if (!runtime_config.allowed_tools.empty()) {
        runtime_config.allowed_tools.push_back("Agent(" + subagent_type + ")");
    }

    loom::tools::agent::AgentTool tool(runtime_config, 0, registry.get(),
                                       std::move(permission_check), true);
    std::ostringstream input;
    input << R"({"prompt":")" << json_escape(prompt) << R"(","subagent_type":")"
          << json_escape(subagent_type) << R"(","run_in_background":true)"
          << R"(,"name":")" << json_escape(agent_config.name) << R"(","team_name":")"
          << json_escape(team_name) << '"';
    if (!agent_config.system_prompt.empty()) {
        input << R"(,"override_system_prompt":")" << json_escape(agent_config.system_prompt) << '"';
    }
    if (!agent_config.model.empty()) {
        input << R"(,"model":")" << json_escape(agent_config.model) << '"';
    }
    if (agent_config.type == AgentType::Plan) {
        input << R"(,"mode":"plan")";
    }
    auto cwd = resolve_working_dir(std::move(working_dir));
    if (!cwd.empty()) {
        input << R"(,"cwd":")" << json_escape(cwd) << '"';
    }
    input << '}';

    auto tool_input = loom::core::ToolInput::from_json(input.str());
    tool_input.api_context = std::move(api_context);
    tool_input.parent_model = std::move(parent_model);
    auto result = tool.execute(tool_input);
    if (!result) {
        auto error = result.error().format();
        return failed_agent_result(std::move(error));
    }
    auto output = tool_result_text(*result);
    // Team backends return launch receipts, not the eventual remote result.
    return MultiAgentResult{
        .output = std::move(output),
        .turns_used = 0,
        .tokens_used = 0,
        .completed = false,
        .started = !result->is_error,
    };
}
}  // namespace detail

struct MultiAgentConfig {
    std::vector<MultiAgentAgentConfig> agents;
    bool parallel = true;
    std::optional<std::string> coordinator_prompt;
    std::optional<std::string> team_name;
    std::string working_dir;
    bool prefer_in_process = false;
    // The caller freezes the tool pool before spawning; futures retain its lifetime.
    std::shared_ptr<loom::core::ToolRegistry> registry;
    std::shared_ptr<const loom::core::ApiExecutionContext> api_context = {};
    std::optional<std::string> parent_model = std::nullopt;
    // Callbacks are copied into futures; captured policy state must remain alive
    // and support concurrent calls when parallel is true.
    AgentLivePermissionCheckFn permission_check = {};
};

inline auto spawn_agents(const MultiAgentConfig& config)
    -> std::vector<std::future<MultiAgentResult>> {
    std::vector<std::future<MultiAgentResult>> futures;
    futures.reserve(config.agents.size());

    for (const auto& agent_config : config.agents) {
        const bool teammate = config.team_name && !config.team_name->empty();
        auto prompt = teammate ? detail::teammate_prompt(agent_config, config.coordinator_prompt)
                               : detail::agent_prompt(agent_config, config.coordinator_prompt);
        futures.push_back(std::async(
            config.parallel ? std::launch::async : std::launch::deferred,
            [agent_config, prompt = std::move(prompt), team_name = config.team_name,
             working_dir = config.working_dir, prefer_in_process = config.prefer_in_process,
             registry = config.registry, api_context = config.api_context,
             parent_model = config.parent_model,
             permission_check = config.permission_check]() mutable -> MultiAgentResult {
                if (team_name && !team_name->empty()) {
                    return detail::spawn_teammate_agent(
                        agent_config, *team_name, std::move(prompt), std::move(working_dir),
                        prefer_in_process, registry, std::move(api_context),
                        std::move(parent_model), std::move(permission_check));
                }
                return detail::execute_agent_tool(
                    agent_config, std::move(prompt), std::move(working_dir), registry,
                    std::move(api_context), std::move(parent_model), std::move(permission_check));
            }));
    }

    return futures;
}

inline auto wait_all(std::span<std::future<MultiAgentResult>> futures)
    -> std::vector<MultiAgentResult> {
    std::vector<MultiAgentResult> results;
    results.reserve(futures.size());

    for (auto& fut : futures) {
        results.push_back(fut.get());
    }

    return results;
}

inline auto merge_agent_results(std::span<const MultiAgentResult> results) -> std::string {
    if (results.empty()) {
        return "No agent results to merge.";
    }

    if (results.size() == 1) {
        if (results[0].started && !results[0].completed) {
            return results[0].output + "\n\nAgent started; task completion is pending.";
        }
        return results[0].output;
    }

    std::ostringstream oss;
    oss << "## Multi-Agent Results\n\n";

    int total_tokens = 0;
    int total_turns = 0;
    int completed_count = 0;
    int started_count = 0;

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];

        oss << "### Agent " << (i + 1) << "\n";
        oss << result.output << "\n\n";

        total_tokens += result.tokens_used;
        total_turns += result.turns_used;
        if (result.completed) ++completed_count;
        if (result.started) ++started_count;
    }

    oss << "---\n";
    oss << "**Summary**: " << started_count << "/" << results.size() << " agents started, "
        << completed_count << "/" << results.size() << " agents completed, " << total_turns
        << " total turns, " << total_tokens << " total tokens\n";

    return oss.str();
}

}  // namespace loom::tools
