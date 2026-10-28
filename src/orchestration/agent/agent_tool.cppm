// AgentTool - Sub-agent delegation with recursive API loop
module;

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

export module loom.orchestration.agent;

import std;

import loom.utils.error;
import loom.config.settings;
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
import loom.skills.skill;
import loom.teams.team_helpers;
import loom.services.api.client;
import loom.services.api.streaming;
import loom.services.mcp.types;
import loom.teams.swarm.backends;
import loom.process.bash.bash_execution;

// Sub-modules created during P1-04 split
import loom.orchestration.agent.utils;
import loom.orchestration.agent.run;
import loom.orchestration.agent.fork;
import loom.orchestration.agent.resume;

export namespace loom::tools::agent {

namespace fs = std::filesystem;

using loom::core::InputSchema;
using loom::core::SchemaProperty;
using loom::core::Tool;
using loom::core::ToolDefinition;
using loom::core::ToolInput;
using loom::core::ToolPermission;
using loom::core::ToolResult;
using loom::services::api::ContentBlock;
using loom::services::api::ContentBlockType;
using loom::services::api::CreateMessageRequest;
using loom::services::api::Message;
using loom::services::api::MessagesClient;
using loom::services::api::StreamContentBlockType;
using loom::services::api::StreamEventType;
using loom::services::api::StreamParser;
using loom::utils::Result;

// =========================================================================
// Re-import helpers from sub-modules (P1-04 split)
// =========================================================================

using utils::AgentConfig;
using utils::AgentContentReplacementState;
using utils::AgentExecutionPlan;
using utils::AgentHookContinuationStop;
using utils::AgentHookExecutionResult;
using utils::AgentHookRunResult;
using utils::AgentInlineMcpServerRuntimeState;
using utils::AgentLivePermissionCheck;
using utils::AgentLivePermissionCheckFn;
using utils::AgentMcpCleanupGuard;
using utils::AgentMcpToolBinding;
using utils::AgentShellTaskCleanupGuard;
using utils::AgentTodoCleanupGuard;
using utils::AgentToolHookContext;
using utils::AgentToolRequest;
using utils::AgentToolResultBudgetOutcome;
using utils::AgentToolResultCandidate;
using utils::AgentWorktreeCleanupResult;
using utils::AgentWorktreeInfo;

using utils::add_agent_memory_tools;
using utils::agent_base_filter_allows_tool;
using utils::agent_content_replacement_state_from_entries;
using utils::AGENT_DEFAULT_TOOL_RESULT_THRESHOLD_CHARS;
using utils::AGENT_MAX_TOOL_RESULTS_PER_MESSAGE_CHARS;
using utils::agent_memory_dir;
using utils::agent_output_file_path;
using utils::agent_schema_property;
using utils::agent_tool_input_omits_agent_type;
using utils::agent_type_allowed_by_permission_rules;
using utils::append_agent_sidechain_message;
using utils::append_hook_additional_context_messages;
using utils::append_json_optional_string_field;
using utils::append_json_string_field;
using utils::apply_agent_effort_to_request;
using utils::apply_agent_tool_execution_context_to_input;
using utils::apply_agent_tool_result_budget;
using utils::apply_resume_content_replacements;
using utils::built_in_system_prompt;
using utils::canonical_tool_name;
using utils::cleanup_agent_worktree;
using utils::create_agent_worktree;
using utils::current_session_is_teammate;
using utils::default_agent_transcript_path;
using utils::execute_agent_frontmatter_hooks;
using utils::execute_agent_tool_frontmatter_hooks;
using utils::filter_incomplete_tool_calls;
using utils::filter_resume_orphaned_thinking_messages;
using utils::filter_resume_unresolved_tool_use_messages;
using utils::filter_resume_whitespace_assistant_messages;
using utils::fork_context_messages_from_entries;
using utils::format_agent_runtime_context;
using utils::format_critical_system_reminder;
using utils::format_hook_additional_context_message;
using utils::format_teammate_agent_id;
using utils::format_tool_hook_additional_context;
using utils::inject_agent_id_into_todo_input;
using utils::inject_agent_id_into_tool_input;
using utils::is_agent_scoped_shell_tool_name;
using utils::is_auto_memory_enabled;
using utils::is_mcp_tool_name;
using utils::is_todo_write_tool_name;
using utils::join_fields;
using utils::json_bool;
using utils::json_escape_string;
using utils::json_int;
using utils::json_object_with_string_overrides;
using utils::json_string;
using utils::load_agent_memory_prompt;
using utils::lowercase_ascii;
using utils::lowercase_copy;
using utils::mark_seen_tool_result_ids;
using utils::message_content_sidechain_json;
using utils::message_content_text;
using utils::message_from_json_value;
using utils::message_json_object;
using utils::next_agent_id;
using utils::normalize_agent_cwd;
using utils::normalized_tool_name_in;
using utils::normalized_tool_name_is;
using utils::parse_agent_tool_request;
using utils::prepend_initial_prompt;
using utils::query_source_is_fork_child;
using utils::resolve_agent_model;
using utils::resolve_agent_relative_path;
using utils::resume_content_replacements_from_entries;
using utils::resume_messages_from_sidechain_entries;
using utils::sanitize_teammate_agent_name;
using utils::sanitized_agent_file_part;
using utils::shell_quote;
using utils::teammate_agent_color;
using utils::teammate_parent_session_id;
using utils::teammate_role_for_agent_type;
using utils::text_contains_fork_boilerplate;
using utils::tool_name_allowed_by_definition;
using utils::tool_name_disallowed_by_definition;
using utils::tool_result_budget_thresholds;
using utils::tool_result_content_text;
using utils::tool_rule_matches_tool_name;
using utils::unbounded_tool_result_budget_names;
using utils::unique_teammate_agent_name;
using utils::update_teammate_completion_status;
using utils::upsert_agent_record_for_plan;

using fork_::build_implicit_fork_agent_input_json;
using fork_::exact_tool_names_from_api_tools;
using fork_::exact_tools_allow_tool;
using fork_::forked_messages_from_parent_assistant;
using fork_::forked_messages_from_parent_assistant_entries;
using fork_::message_contains_fork_boilerplate;
using fork_::messages_contain_fork_boilerplate;
using fork_::should_reject_fork_child_agent_call;
using loom::tools::agent::utils::agent_hook_output_preview;

using resume_::format_resumed_agent_context;
using resume_::hydrate_resume_plan_from_existing_record;

using run_::build_agent_execution_plan;
using run_::execute_agent_sleep_tool;
using run_::execute_agent_web_fetch_tool;

// TODO(agent-split): refine placement — ambiguous dependency
// (symbols above left in agent_tool root via using-import from submodules)

// =========================================================================
// AgentTool Implementation
// =========================================================================

/// AgentTool - Delegates tasks to sub-agents via recursive API loop
class AgentTool {
public:
    static constexpr std::string_view kName = "Agent";
    static constexpr std::string_view kDescription =
        "Delegate substantial independent research, review, or implementation work "
        "when a separate scoped task improves coverage or latency. Give a clear "
        "objective, relevant context, permitted scope, and expected evidence; the "
        "child does not inherit the full parent conversation. Select subagent_type "
        "general-purpose unless another type is known to be available. Use "
        "run_in_background=true for concurrent work, continue useful non-duplicating "
        "work, then collect and verify the final result. A successful launch is not "
        "task completion. Keep simple or tightly dependent steps local, and preserve "
        "the user's and repository's constraints.";

    static ToolDefinition definition() {
        return ToolDefinition{
            .name = std::string(kName),
            .description = std::string(kDescription),
            .input_schema =
                InputSchema{
                    .properties =
                        {agent_schema_property("description", "string",
                                               "A short description of the task", true),
                         agent_schema_property(
                             "prompt", "string",
                             "Self-contained task: objective, relevant context, permitted scope, "
                             "expected output, and verification evidence",
                             true),
                         agent_schema_property(
                             "subagent_type", "string",
                             "Explicit agent type; use general-purpose for ordinary delegation. "
                             "Other types must be available in this session"),
                         agent_schema_property("model", "string",
                                               "Optional model override: sonnet, opus, haiku, or a "
                                               "concrete model id"),
                         agent_schema_property(
                             "run_in_background", "boolean",
                             "Start concurrently and return an agent ID immediately; collect its "
                             "final status and output before relying on the result"),
                         agent_schema_property("task", "string", "Legacy alias for prompt"),
                         agent_schema_property("skill", "string", "Legacy alias for subagent_type"),
                         agent_schema_property(
                             "name", "string",
                             "Optional agent display name; with team_name, the teammate name"),
                         agent_schema_property("team_name", "string", "Team name for agent swarms"),
                         agent_schema_property("mode", "string",
                                               "Permission mode for spawned teammates"),
                         agent_schema_property("isolation", "string",
                                               "Isolation mode for worktree agents"),
                         agent_schema_property(
                             "cwd", "string", "Working directory override for the spawned agent")}},
            .permission = ToolPermission::Execute,
            .category = "agent"};
    }

    explicit AgentTool(AgentConfig config = {}, int current_depth = 0,
                       loom::core::ToolRegistry* registry = nullptr,
                       AgentLivePermissionCheckFn permission_check = {},
                       bool permission_hook_valid_for_background = false)
        : config_(config)
        , current_depth_(current_depth)
        , registry_(registry)
        , permission_check_(std::move(permission_check))
        , permission_hook_valid_for_background_(permission_hook_valid_for_background) {
        if (config_.default_model.empty()) {
            config_.default_model = loom::config::resolve_default_model_from_environment(
                [](std::string_view name) -> std::optional<std::string> {
                    const auto key = std::string(name);
                    if (const char* value = std::getenv(key.c_str()); value && *value) {
                        return std::string(value);
                    }
                    return std::nullopt;
                });
        }
        if (registry_) {
            auto& store = loom::tools::agent_runtime::native_agent_store();
            registry_->set_shutdown_handler([&store](std::stop_token owner) {
                store.cancel_and_wait_for_background_agents(owner);
            });
        }
    }

    [[nodiscard]] bool check_permission(const ToolInput& input) const {
        (void)input;
        // Deny if at max recursion depth
        if (current_depth_ >= config_.max_depth) return false;
        return true;
    }

    /// Check if a specific tool is allowed for this sub-agent
    [[nodiscard]] bool is_tool_allowed(std::string_view tool_name) const {
        // Check denied list first (takes precedence)
        for (const auto& denied : config_.denied_tools) {
            if (tool_rule_matches_tool_name(denied, tool_name)) return false;
        }
        // Agent(type) rules gate Agent invocations, not the spawned worker's tool pool.
        if (!config_.allowed_tools.empty()) {
            bool has_tool_allow_rule = false;
            for (const auto& allowed : config_.allowed_tools) {
                if (!normalized_tool_name_is(tool_name, "Agent") &&
                    tool_rule_matches_tool_name(allowed, "Agent")) {
                    continue;
                }
                has_tool_allow_rule = true;
                if (tool_rule_matches_tool_name(allowed, tool_name)) return true;
            }
            return !has_tool_allow_rule;
        }
        return true;  // No restrictions = allow all
    }

    /// Get config for creating child agents (depth incremented)
    [[nodiscard]] AgentConfig
    child_config(std::string_view parent_agent_id = {},
                 const std::optional<std::string>& parent_permission_mode = std::nullopt,
                 const std::optional<std::string>& parent_model = std::nullopt) const {
        AgentConfig child = config_;
        if (parent_model) child.default_model = *parent_model;
        // Children inherit tool restrictions
        if (!parent_agent_id.empty()) {
            child.parent_agent_id = std::string(parent_agent_id);
        }
        if (parent_permission_mode) {
            child.parent_permission_mode = parent_permission_mode;
        }
        return child;
    }

    [[nodiscard]] int current_depth() const { return current_depth_; }

    [[nodiscard]] int max_depth() const { return config_.max_depth; }

    [[nodiscard]] AgentLivePermissionCheck
    check_live_tool_permission(std::string_view tool_name, std::string_view input_json,
                               std::string_view tool_use_id) {
        if (!permission_check_) return AgentLivePermissionCheck{};
        return permission_check_(tool_name, input_json, tool_use_id);
    }

    // Last-call measurements are meaningful after exclusive execution. Shared
    // registry adapters may execute concurrently; metrics never serialize work.
    [[nodiscard]] int last_run_turns() const noexcept;
    [[nodiscard]] int last_run_tokens() const noexcept;

    [[nodiscard]] Result<ToolResult> execute(const ToolInput& input);

private:
    [[nodiscard]] Result<ToolResult> start_teammate_agent(AgentExecutionPlan plan) {
        if (!plan.team_name || plan.team_name->empty() || !plan.name || plan.name->empty()) {
            return ToolResult::error("teammate spawn requires team_name and name");
        }

        auto team = loom::tools::global_team_store().get_by_id_or_name(*plan.team_name);
        if (!team) {
            return ToolResult::error(
                std::format("Team '{}' not found. Call team_create before spawning teammates.",
                            *plan.team_name));
        }

        auto normalized_team_name = team->name;
        auto team_id = team->id;
        auto teammate_name =
            unique_teammate_agent_name(sanitize_teammate_agent_name(*plan.name), *team);
        auto teammate_id = format_teammate_agent_id(teammate_name, normalized_team_name);
        auto output_file = agent_output_file_path(teammate_id);

        plan.agent_id = teammate_id;
        plan.name = teammate_name;
        plan.team_name = normalized_team_name;
        plan.background = true;

        auto added = loom::tools::global_team_store().add_member(
            team_id, loom::tools::TeamMember{
                         .agent_id = teammate_id,
                         .role = teammate_role_for_agent_type(plan.agent_type),
                         .status = loom::tools::MemberStatus::Working,
                     });
        if (!added) return ToolResult::error(std::string(loom::tools::format_error(added.error())));

        loom::utils::swarm_backends::TeammateSpawnConfig spawn_config{
            .name = teammate_name,
            .team_name = normalized_team_name,
            .color = teammate_agent_color(plan.color),
            .plan_mode_required = plan.mode == "plan",
            .permission_mode = plan.mode,
            .agent_type = plan.agent_type,
            .prompt = plan.prompt,
            .cwd = plan.working_dir.value_or(fs::current_path().string()),
            .model = plan.model.empty() ? std::nullopt : std::optional<std::string>{plan.model},
            .system_prompt = plan.system_prompt.empty()
                                 ? std::nullopt
                                 : std::optional<std::string>{plan.system_prompt},
            .worktree_path = plan.worktree_path,
            .parent_session_id = plan.parent_session_id && !plan.parent_session_id->empty()
                                     ? *plan.parent_session_id
                                     : teammate_parent_session_id(),
            .permissions = plan.allowed_tools,
        };
        auto executor = loom::utils::swarm_backends::BackendRegistry::get_teammate_executor(
            config_.prefer_in_process_teammate);
        auto spawned = executor->spawn(spawn_config);
        if (!spawned.success) {
            auto error = spawned.error.value_or("failed to spawn teammate backend");
            (void)loom::tools::global_team_store().update_member_status(
                team_id, teammate_id, loom::tools::MemberStatus::Error, error);
            return ToolResult::error(error);
        }

        auto backend =
            std::string(loom::utils::swarm_backends::backend_type_name(executor->type()));
        auto task_id = std::move(spawned.task_id);
        auto pane_id = std::move(spawned.pane_id);
        auto color = spawn_config.color
                         ? std::optional<std::string>{std::string(
                               loom::utils::swarm_backends::agent_color_name(*spawn_config.color))}
                         : std::nullopt;
        auto parent_session_id = std::move(spawn_config.parent_session_id);

        plan.teammate_backend = backend;
        plan.teammate_task_id = task_id;
        plan.teammate_pane_id = pane_id;
        plan.teammate_color = color;
        plan.parent_session_id = parent_session_id;

        const bool runs_in_process =
            executor->type() == loom::utils::swarm_backends::BackendType::InProcess;
        if (runs_in_process) {
            auto started = start_background_agent(std::move(plan));
            if (!started) return started;
            if (started->is_error) return started;
        } else {
            auto mailbox = loom::utils::write_to_mailbox(
                teammate_name,
                loom::utils::TeammateMessage{
                    .from = "team-lead",
                    .text = plan.prompt,
                    .timestamp = {},
                    .read = false,
                    .color = std::nullopt,
                    .summary = "initial teammate instructions",
                },
                std::optional<std::string_view>{std::string_view(normalized_team_name)});
            if (!mailbox) {
                (void)loom::tools::global_team_store().update_member_status(
                    team_id, teammate_id, loom::tools::MemberStatus::Error, mailbox.error());
                return ToolResult::error("failed to write teammate mailbox: " + mailbox.error());
            }

            loom::tools::MessageRouter::instance().register_agent(plan.agent_id);
            upsert_agent_record_for_plan(plan);
            loom::tools::agent_runtime::native_agent_store().mark_running(plan.agent_id);
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, std::format("system: spawned teammate via {} backend", backend));
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "mailbox initial prompt: " + plan.prompt);
        }

        auto task_line = task_id ? std::format("task_id: {}\n", *task_id) : std::string{};
        auto pane_line = pane_id ? std::format("pane_id: {}\n", *pane_id) : std::string{};
        auto color_line = color ? std::format("color: {}\n", *color) : std::string{};
        auto execution_state = runs_in_process
                                   ? (registry_ ? std::string{"running"} : std::string{"queued"})
                                   : std::string{"running in the external teammate backend"};
        auto delivery_channel =
            runs_in_process ? std::string{"native runtime queue"} : std::string{"teammate mailbox"};
        return ToolResult::success(std::format(
            "Spawned successfully.\n"
            "agent_id: {}\n"
            "teammate_id: {}\n"
            "name: {}\n"
            "team_name: {}\n"
            "backend: {}\n"
            "{}{}{}"
            "status: teammate_spawned\n"
            "outputFile: {}\n"
            "The agent is {} and will receive instructions via the {}.",
            teammate_id, teammate_id, teammate_name, normalized_team_name, backend, task_line,
            pane_line, color_line, output_file, execution_state, delivery_channel));
    }

    [[nodiscard]] Result<ToolResult> start_background_agent(AgentExecutionPlan plan);

    [[nodiscard]] bool plan_allows_generic_mcp_tool(const AgentExecutionPlan& plan) const {
        return !plan.agent_mcp_tools.empty() && is_tool_allowed("mcp") &&
               !tool_name_disallowed_by_definition("mcp", plan.disallowed_tools);
    }

    [[nodiscard]] bool agent_base_filter_allows_tool(std::string_view tool_name,
                                                     const AgentExecutionPlan& plan) const {
        const auto permission_mode =
            plan.mode ? std::optional<std::string_view>{std::string_view{*plan.mode}}
                      : std::nullopt;
        const bool in_process_teammate =
            plan.name && !plan.name->empty() && plan.team_name && plan.teammate_backend &&
            normalized_tool_name_is(*plan.teammate_backend, "in-process");
        return loom::tools::agent::agent_base_filter_allows_tool(
            tool_name, plan.is_built_in, plan.background, permission_mode, in_process_teammate);
    }

    [[nodiscard]] bool is_tool_allowed_for_plan(std::string_view tool_name,
                                                const AgentExecutionPlan& plan) const {
        if (tool_name == "mcp" && plan_allows_generic_mcp_tool(plan)) {
            return true;
        }
        return agent_base_filter_allows_tool(tool_name, plan) && is_tool_allowed(tool_name) &&
               tool_name_allowed_by_definition(tool_name, plan.allowed_tools) &&
               !tool_name_disallowed_by_definition(tool_name, plan.disallowed_tools);
    }

    [[nodiscard]] std::optional<std::string>
    mcp_scope_error_for_plan(const ToolInput& input, const AgentExecutionPlan& plan) const {
        if (plan.agent_mcp_servers.empty()) return std::nullopt;

        auto doc = loom::utils::json::parse(input.json());
        if (!doc || !doc->root().is_obj()) {
            return "MCP input must be a JSON object in this sub-agent context";
        }

        auto root = doc->root();
        auto server =
            json_string(root, "server_name").or_else([&] { return json_string(root, "server"); });
        if (!server || server->empty()) {
            return "MCP server_name is required in this sub-agent context";
        }

        for (const auto& allowed : plan.agent_mcp_servers) {
            if (*server == allowed) return std::nullopt;
        }
        return std::format(
            "MCP server '{}' is not available in this sub-agent context. Available servers: {}",
            *server, join_fields(plan.agent_mcp_servers));
    }

    [[nodiscard]] std::vector<loom::services::api::ToolDefinition>
    api_tools_for_plan(const AgentExecutionPlan& plan) const {
        std::vector<loom::services::api::ToolDefinition> tools;
        if (!registry_) return tools;

        for (const auto& definition : registry_->get_visible_definitions()) {
            if (plan.use_exact_tools) {
                if (!exact_tools_allow_tool(plan, definition.name)) continue;
            } else if (!is_tool_allowed_for_plan(definition.name, plan)) {
                continue;
            }
            tools.push_back(loom::services::api::ToolDefinition{
                .name = definition.name,
                .description = definition.description,
                .input_schema_json = definition.input_schema.to_json(),
                .defer_load = false,
            });
        }
        return tools;
    }

    void append_queued_agent_messages(const AgentExecutionPlan& plan,
                                      std::vector<Message>& messages,
                                      std::optional<std::size_t>& request_anchor) const;

    /// All executions publish their terminal state after the loop's resources
    /// and worktree have been cleaned up, then project it to team status.
    /// Startup failures retain
    /// their synchronous ToolResult error while using the same cleanup ordering.
    [[nodiscard]] Result<ToolResult>
    run_agent_and_finalize(const AgentExecutionPlan& plan,
                           std::optional<std::string> startup_error = std::nullopt,
                           std::function<bool()> parent_is_cancelled = {});

    /// Run the sub-agent's recursive API loop
    [[nodiscard]] Result<ToolResult> run_agent_loop(const AgentExecutionPlan& plan);

    AgentConfig config_;
    void record_run_usage(int input_tokens, int output_tokens) noexcept;
    alignas(std::atomic_ref<int>::required_alignment) mutable int last_run_turns_ = 0;
    alignas(std::atomic_ref<int>::required_alignment) mutable int last_run_tokens_ = 0;
    int current_depth_ = 0;
    loom::core::ToolRegistry* registry_ = nullptr;
    AgentLivePermissionCheckFn permission_check_;
    bool permission_hook_valid_for_background_ = false;
};

}  // namespace loom::tools::agent

// Export main tool class
export namespace loom::tools {
using loom::tools::agent::AgentConfig;
using loom::tools::agent::AgentLivePermissionCheck;
using loom::tools::agent::AgentLivePermissionCheckFn;
using loom::tools::agent::AgentTool;

/// Factory: create AgentTool wrapped as ITool (adapts Result types across modules)
[[nodiscard]] auto make_agent_tool(AgentConfig config = {}, int depth = 0,
                                   loom::core::ToolRegistry* registry = nullptr,
                                   AgentLivePermissionCheckFn permission_check = {},
                                   bool permission_hook_valid_for_background = false)
    -> std::unique_ptr<loom::core::ITool> {
    struct Adapter final : loom::core::ITool {
        AgentTool tool_;
        loom::core::ToolDefinition def_ = AgentTool::definition();

        explicit Adapter(AgentConfig cfg, int d, loom::core::ToolRegistry* reg,
                         AgentLivePermissionCheckFn permission_check,
                         bool hook_valid_for_background)
            : tool_(std::move(cfg), d, reg, std::move(permission_check),
                    hook_valid_for_background) {}

        const loom::core::ToolDefinition& definition() const override { return def_; }

        std::expected<loom::core::ToolResult, loom::core::Error>
        execute(const loom::core::ToolInput& input) override {
            auto result = tool_.execute(input);
            if (result) return std::move(*result);
            return std::unexpected(loom::core::Error::make(
                loom::core::ErrorCode::ToolExecutionFailed, result.error().format()));
        }

        bool check_permission(const loom::core::ToolInput& input) const override {
            return tool_.check_permission(input);
        }
    };

    return std::make_unique<Adapter>(std::move(config), depth, registry,
                                     std::move(permission_check),
                                     permission_hook_valid_for_background);
}
}  // namespace loom::tools
