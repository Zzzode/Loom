// Agent finalization for loom.orchestration.agent.
module;

module loom.orchestration.agent;

import std;

namespace loom::tools::agent {

[[nodiscard]] Result<ToolResult> AgentTool::run_agent_and_finalize(
    const AgentExecutionPlan& plan,
    std::optional<std::string> startup_error) {
    auto& store = loom::tools::agent_runtime::native_agent_store();
    struct ExecutionGuard {
        loom::tools::agent_runtime::NativeAgentStore& store;
        const std::string& id;
        bool active = false;
        ~ExecutionGuard() { if (active) store.end_execution(id); }
    } execution{store, plan.agent_id};
    Result<ToolResult> result = ToolResult::error("Agent execution did not start");
    {
        AgentTodoCleanupGuard todo_cleanup{plan.agent_id};
        AgentShellTaskCleanupGuard shell_task_cleanup{plan.agent_id};
        AgentMcpCleanupGuard mcp_cleanup{
            .agent_id = plan.agent_id,
            .inline_servers = plan.inline_mcp_server_states,
        };
        try {
            if (!plan.background) {
                upsert_agent_record_for_plan(plan,
                    registry_ ? registry_->background_stop_token() : std::stop_token{});
                execution.active = true;
            }
            result = startup_error ? ToolResult::error(*startup_error) : run_agent_loop(plan);
        } catch (const std::exception& error) {
            result = ToolResult::error(error.what());
        } catch (...) {
            result = ToolResult::error("Agent execution failed");
        }
    }
    (void)cleanup_agent_worktree(plan.agent_id);
    const bool succeeded = result && !result->is_error;
    auto text = result ? tool_result_content_text(*result) : result.error().format();
    if (succeeded) store.finish_agent(plan.agent_id, text);
    else store.finish_agent(plan.agent_id, std::unexpected(text));
    // Team status projects the stable native terminal state. A cancellation
    // racing finalization must not leave a cancelled agent marked Done.
    const auto record = store.get(plan.agent_id);
    const bool completed = record &&
        record->status == loom::tools::agent_runtime::NativeAgentStatus::Completed;
    update_teammate_completion_status(plan, completed,
        record ? (completed ? record->output.value_or(text) : record->error.value_or(text)) : text);
    if (record && record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled) {
        return ToolResult::error(record->error.value_or("Agent cancelled"));
    }
    return result;
}

} // namespace loom::tools::agent
