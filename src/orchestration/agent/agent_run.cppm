module;

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>

export module loom.orchestration.agent.run;

import std;

import loom.utils.error;
import loom.scm.git.git;
import loom.tools.tool;
import loom.serdes.json;
import loom.tools.agent_runtime;
import loom.tools.agent_constants;
import loom.tools.bash;
import loom.tools.todo_write;
import loom.tools.send_message;
import loom.tools.team;
import loom.orchestration.tools.mcp;
import loom.tools.sleep;
import loom.tools.web_fetch;
import loom.skills.skill;
import loom.teams.team_helpers;
import loom.services.api.client;
import loom.services.api.streaming;
import loom.teams.swarm.backends;
import loom.process.bash.bash_execution;
import loom.orchestration.agent.utils;
import loom.orchestration.agent.fork;
import loom.orchestration.agent.resume;

export namespace loom::tools::agent::run_ {

namespace fs = std::filesystem;

using loom::core::Tool;
using loom::core::ToolInput;
using loom::core::ToolResult;
using loom::core::ToolDefinition;
using loom::core::ToolPermission;
using loom::core::InputSchema;
using loom::core::SchemaProperty;
using loom::utils::Result;
using loom::services::api::AnthropicClient;
using loom::services::api::CreateMessageRequest;
using loom::services::api::Message;
using loom::services::api::ContentBlock;
using loom::services::api::ContentBlockType;
using loom::services::api::StreamParser;
using loom::services::api::StreamEventType;
using loom::services::api::StreamContentBlockType;
using loom::tools::agent::utils::AgentToolRequest;
using loom::tools::agent::utils::AgentConfig;
using loom::tools::agent::utils::AgentExecutionPlan;
using loom::tools::agent::utils::AgentMcpToolBinding;
using loom::tools::agent::utils::AgentInlineMcpServerRuntimeState;
using loom::tools::agent::utils::AgentMcpCleanupGuard;
using loom::tools::agent::utils::normalize_agent_cwd;
using loom::tools::agent::utils::agent_schema_property;
using loom::tools::agent::utils::resolve_agent_model;
using loom::tools::agent::utils::prepend_initial_prompt;
using loom::tools::agent::utils::built_in_system_prompt;
using loom::tools::agent::utils::next_agent_id;
using loom::tools::agent::utils::load_preloaded_skill_messages;
using loom::tools::agent::utils::format_agent_mcp_context_message;
using loom::tools::agent::utils::connect_agent_mcp_servers;
using loom::tools::agent::utils::append_unique_agent_mcp_server;
using loom::tools::agent::utils::prepare_agent_inline_mcp_servers;
using loom::tools::agent::utils::available_mcp_servers_with_tools;
using loom::tools::agent::utils::missing_required_mcp_servers;
using loom::tools::agent::utils::join_fields;
using loom::tools::agent::utils::effective_agent_permission_mode;
using loom::tools::agent::utils::filter_incomplete_tool_calls;
using loom::tools::agent::utils::is_auto_memory_enabled;
using loom::tools::agent::utils::json_int;
using loom::tools::agent::utils::fork_context_messages_from_entries;
using loom::tools::agent::utils::add_agent_memory_tools;
using loom::tools::agent::utils::load_agent_memory_prompt;
using loom::tools::agent::utils::format_critical_system_reminder;
using loom::tools::agent::utils::format_agent_runtime_context;

using loom::tools::agent::fork_::forked_messages_from_parent_assistant_entries;
using loom::tools::agent::fork_::messages_contain_fork_boilerplate;
using loom::tools::agent::resume_::hydrate_resume_plan_from_existing_record;

[[nodiscard]] inline std::expected<AgentExecutionPlan, std::string> build_agent_execution_plan(
    const AgentToolRequest& request,
    const AgentConfig& config
) {
    auto normalized_cwd = normalize_agent_cwd(request.cwd);
    if (!normalized_cwd) return std::unexpected(normalized_cwd.error());

    const auto agents = loom::tools::agent_runtime::get_all_agent_definitions(
        normalized_cwd->has_value() ? std::optional<fs::path>{fs::path{**normalized_cwd}} : std::nullopt);

    // migrated edge case: use resolve_agent_type (std::expected wrapper) so
    // callers get a richer error category. Legacy alias ambiguity and other
    // failure modes are translated to descriptive strings; previous code used
    // the simpler "not found" surface.
    auto resolved_definition = loom::tools::agent_runtime::resolve_agent_type(request.subagent_type, agents);
    if (!resolved_definition) {
        const auto error_kind = loom::tools::agent_runtime::resolution_error_name(resolved_definition.error());
        if (resolved_definition.error() == loom::tools::agent_runtime::ResolutionError::LegacyAliasAmbiguous) {
            return std::unexpected(std::format(
                "Agent type '{}' matched multiple namespaced variants; "
                "specify the fully-qualified agent type explicitly. Available agents: {}",
                request.subagent_type,
                loom::tools::agent_runtime::format_agent_type_list(agents)));
        }
        return std::unexpected(std::format(
            "Agent type '{}' not found ({}). Available agents: {}",
            request.subagent_type,
            error_kind,
            loom::tools::agent_runtime::format_agent_type_list(agents)));
    }
    loom::tools::agent_runtime::AgentDefinition definition = std::move(*resolved_definition);

    auto inline_mcp_servers = prepare_agent_inline_mcp_servers(definition.inline_mcp_servers);
    if (!inline_mcp_servers) {
        return std::unexpected(std::format(
            "Failed to configure MCP servers for agent '{}': {}",
            definition.agent_type,
            inline_mcp_servers.error()));
    }
    AgentMcpCleanupGuard inline_mcp_plan_cleanup{
        .agent_id = {},
        .inline_servers = std::move(*inline_mcp_servers),
    };

    auto definition_model = resolve_agent_model(definition.model);
    AgentExecutionPlan plan;
    plan.agent_id = request.agent_id_override.value_or(next_agent_id(request.name));
    plan.prompt = prepend_initial_prompt(definition.initial_prompt, request.prompt);
    plan.description = request.description;
    plan.agent_type = definition.agent_type;
    plan.is_built_in = definition.source == "built-in";
    plan.model = request.model.value_or(definition_model.value_or(config.default_model));
    plan.system_prompt = definition.system_prompt.empty()
        ? built_in_system_prompt(definition.agent_type)
        : definition.system_prompt;
    if (request.parent_system_prompt && !request.parent_system_prompt->empty()) {
        plan.system_prompt = *request.parent_system_prompt;
        plan.system_prompt_overridden = true;
    }
    plan.preloaded_skill_messages = load_preloaded_skill_messages(definition);
    plan.agent_mcp_servers = definition.mcp_servers;
    for (const auto& inline_config : definition.inline_mcp_servers) {
        append_unique_agent_mcp_server(plan.agent_mcp_servers, inline_config.name);
    }
    plan.agent_mcp_tools = connect_agent_mcp_servers(plan.agent_mcp_servers);
    if (!plan.agent_mcp_tools.empty()) {
        plan.agent_mcp_context_message = format_agent_mcp_context_message(plan.agent_mcp_tools);
    }
    if (!definition.required_mcp_servers.empty()) {
        const auto available_servers = available_mcp_servers_with_tools();
        const auto missing = missing_required_mcp_servers(definition.required_mcp_servers, available_servers);
        if (!missing.empty()) {
            return std::unexpected(std::format(
                "Agent '{}' requires MCP servers matching: {}. MCP servers with tools: {}. Use /mcp to configure and authenticate the required MCP servers.",
                definition.agent_type,
                join_fields(missing),
                available_servers.empty() ? "none" : join_fields(available_servers)));
        }
    }
    plan.inline_mcp_server_states = std::move(inline_mcp_plan_cleanup.inline_servers);
    plan.allowed_tools = definition.tools;
    plan.disallowed_tools = definition.disallowed_tools;
    plan.max_turns = definition.max_turns.value_or(config.max_turns);
    plan.background = request.run_in_background || definition.background;
    plan.resume_existing = request.resume_existing;
    plan.query_source = request.query_source;
    plan.fork_child_context = request.fork_child_context;
    plan.exact_tools = request.exact_tools;
    plan.use_exact_tools = request.use_exact_tools;
    plan.fork_context_messages = forked_messages_from_parent_assistant_entries(
        request.prompt,
        request.parent_assistant_message_entries);
    if (!plan.fork_context_messages.empty()) {
        plan.fork_child_context = true;
        plan.fork_context_includes_prompt = true;
    }
    auto explicit_fork_context_messages = fork_context_messages_from_entries(request.fork_context_entries);
    plan.fork_context_messages.insert(
        plan.fork_context_messages.end(),
        std::make_move_iterator(explicit_fork_context_messages.begin()),
        std::make_move_iterator(explicit_fork_context_messages.end()));
    // migrated edge case: filter out any assistant messages whose tool_use
    // blocks lack corresponding tool_results. Without this the Anthropic
    // API rejects the request outright ("message with tool_use must be
    // followed by user message with tool_result"). Mirrors TS
    // filterIncompleteToolCalls applied to fork_context_messages.
    if (!plan.fork_context_messages.empty()) {
        plan.fork_context_messages = filter_incomplete_tool_calls(
            std::move(plan.fork_context_messages));
    }
    if (!plan.fork_context_includes_prompt &&
        messages_contain_fork_boilerplate(plan.fork_context_messages)) {
        plan.fork_context_includes_prompt = true;
    }
    if (!plan.fork_child_context) {
        if (auto existing = loom::tools::agent_runtime::native_agent_store().get(plan.agent_id);
            existing && loom::tools::agent_runtime::native_agent_record_is_fork_child(*existing)) {
            plan.fork_child_context = true;
        }
    }
    plan.name = request.name;
    plan.team_name = request.team_name;
    plan.mode = effective_agent_permission_mode(
        request.mode,
        definition.permission_mode,
        config.parent_permission_mode);
    plan.isolation = request.isolation.or_else([&] { return definition.isolation; });
    if (plan.isolation && !loom::tools::agent_runtime::valid_agent_isolation(*plan.isolation)) {
        return std::unexpected(std::format(
            "Unsupported isolation mode '{}'. Valid options for this environment: {}",
            *plan.isolation,
            loom::tools::agent_runtime::valid_agent_isolation_options()));
    }
    plan.working_dir = *normalized_cwd;
    plan.frontmatter_hooks = definition.hooks;
    plan.effort = definition.effort;
    plan.memory = definition.memory;
    if (plan.memory && is_auto_memory_enabled()) {
        add_agent_memory_tools(plan.allowed_tools);
        plan.system_prompt = std::format(
            "{}\n\n{}",
            plan.system_prompt,
            load_agent_memory_prompt(plan.agent_type, *plan.memory, plan.working_dir));
    }
    plan.color = definition.color;
    plan.omit_loom_md = definition.omit_loom_md;
    plan.critical_system_reminder = definition.critical_system_reminder;
    plan.parent_agent_id = config.parent_agent_id;
    hydrate_resume_plan_from_existing_record(plan);
    const auto runtime_context = format_agent_runtime_context(plan);
    if (!plan.system_prompt_overridden) {
        plan.system_prompt = plan.system_prompt.empty()
            ? runtime_context
            : std::format("{}\n\n{}", plan.system_prompt, runtime_context);
    }
    if (plan.critical_system_reminder && !plan.system_prompt_overridden) {
        plan.system_prompt += "\n\n" + format_critical_system_reminder(*plan.critical_system_reminder);
    }
    return plan;
}

[[nodiscard]] inline ToolResult execute_agent_sleep_tool(
    std::string_view agent_id,
    const ToolInput& input
) {
    auto parsed = loom::utils::json::parse(input.json());
    if (!parsed || !parsed->root().is_obj()) {
        return ToolResult::error("sleep requires a JSON object input");
    }

    auto root = parsed->root();
    const auto seconds = std::clamp(
        json_int(root, "duration")
            .or_else([&] { return json_int(root, "seconds"); })
            .value_or(1),
        1,
        static_cast<int>(SleepTool::kMaxDuration.count()));
    auto reason = json_string(root, "reason").value_or("scheduled wait");

    auto signal = std::make_shared<AbortSignal>();
    // Pre-check: if cancel was already requested before we got here, abort immediately.
    if (loom::tools::agent_runtime::native_agent_store().is_cancel_requested(std::string(agent_id))) {
        signal->abort();
    }
    std::atomic_bool finished{false};
    std::thread watcher([signal, agent_id = std::string(agent_id), &finished] {
        while (!finished.load(std::memory_order_acquire)) {
            if (loom::tools::agent_runtime::native_agent_store().is_cancel_requested(agent_id)) {
                signal->abort();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    SleepTool tool(signal);
    auto result = tool.execute(SleepRequest{
        .duration = std::chrono::seconds(seconds),
        .reason = std::move(reason),
        .resume_hint = json_string(root, "resume_hint"),
    });

    finished.store(true, std::memory_order_release);
    if (watcher.joinable()) watcher.join();

    if (!result) {
        return ToolResult::error(std::string(format_error(result.error())));
    }
    if (result->was_cancelled) {
        return ToolResult::error(std::format(
            "Sleep cancelled after {} ms", result->actual_duration.count()));
    }
    return ToolResult::success(std::format("Slept for {} ms", result->actual_duration.count()));
}

[[nodiscard]] inline ToolResult execute_agent_web_fetch_tool(
    std::string_view agent_id,
    const ToolInput& input
) {
    auto parsed_url = loom::tools::web_fetch::detail::parse_url(input.json());
    if (!parsed_url) {
        return ToolResult::error(parsed_url.error());
    }

    const auto& url = *parsed_url;
    if (!url.starts_with("http://") && !url.starts_with("https://")) {
        return ToolResult::error("URL must start with http:// or https://");
    }

    auto result = loom::tools::web_fetch::detail::run_curl_with_cancel(
        url,
        [agent_id = std::string(agent_id)] {
            return loom::tools::agent_runtime::native_agent_store().is_cancel_requested(agent_id);
        });
    if (!result) return ToolResult::error(result.error());
    if (result->cancelled) {
        return ToolResult::error("WebFetch cancelled by agent stop request");
    }
    if (result->exit_status != 0) {
        return ToolResult::error(std::format("Failed to fetch URL: {}", url));
    }
    return ToolResult::success(std::move(result->output));
}

} // namespace loom::tools::agent::run_
