// Agent finalization for loom.orchestration.agent.
module;

module loom.orchestration.agent;

import std;

namespace loom::tools::agent {

Result<ToolResult> AgentTool::execute(const ToolInput& input) {
    std::atomic_ref<int>(last_run_turns_).store(0, std::memory_order_relaxed);
    std::atomic_ref<int>(last_run_tokens_).store(0, std::memory_order_relaxed);
    auto request = parse_agent_tool_request(input);
    if (!request) return ToolResult::error(std::format("Invalid Agent input: {}", request.error()));

    if (request->prompt.empty()) {
        return ToolResult::error("Missing required 'prompt' field");
    }

    auto plan = build_agent_execution_plan(*request, config_, input.parent_model);
    if (!plan) return ToolResult::error(plan.error());
    plan->api_context = input.api_context ? input.api_context : config_.api_context;
    if (input.parent_session_id) plan->parent_session_id = input.parent_session_id;

    if (!plan->resume_existing && !plan->team_name) {
        if (auto current_team = loom::utils::get_team_name(); current_team && !current_team->empty()) {
            plan->team_name = std::move(*current_team);
        }
    }

    if (current_session_is_teammate() && plan->team_name && plan->name) {
        return ToolResult::error(
            "Teammates cannot spawn other teammates - the team roster is flat. "
            "To spawn a subagent instead, omit the name parameter.");
    }

    if (loom::utils::is_in_process_teammate() && plan->team_name && plan->background) {
        return ToolResult::error(
            "In-process teammates cannot spawn background agents. "
            "Use run_in_background=false for synchronous subagents.");
    }

    if (!agent_type_allowed_by_permission_rules(
            plan->agent_type,
            config_.allowed_tools,
            config_.denied_tools)) {
        return ToolResult::error(std::format(
            "Agent type '{}' is not allowed by current Agent tool permission rules",
            plan->agent_type));
    }

    // Check recursion depth
    if (current_depth_ >= config_.max_depth) {
        return ToolResult::error(std::format(
            "Agent recursion depth limit reached ({}/{})",
            current_depth_, config_.max_depth));
    }

    if (!plan->resume_existing && plan->team_name && plan->name && plan->isolation) {
        return ToolResult::error("teammate spawn does not support isolation; omit name/team_name to spawn an isolated subagent");
    }

    if (!plan->resume_existing && plan->team_name && plan->name) {
        return start_teammate_agent(std::move(*plan));
    }

    if (!plan->resume_existing && plan->isolation) {
        if (*plan->isolation == "worktree") {
            const auto parent_working_dir = plan->working_dir.value_or(fs::current_path().string());
            auto worktree = create_agent_worktree(*plan);
            if (!worktree) return ToolResult::error(worktree.error());
            plan->working_dir = worktree->path.string();
            plan->worktree_path = worktree->path.string();
            plan->worktree_branch = worktree->branch;
            plan->worktree_base_commit = worktree->head_commit;
            plan->worktree_git_root = worktree->git_root.string();
            if (plan->fork_context_includes_prompt) {
                plan->fork_context_messages.push_back(Message::from_text(
                    "user",
                    loom::tools::agent_runtime::build_worktree_fork_notice(
                        parent_working_dir,
                        worktree->path.string())));
            } else {
                plan->system_prompt += std::format(
                    "\n\nNative worktree isolation:\n"
                    "- worktree_path: {}\n"
                    "- worktree_branch: {}\n"
                    "- base_commit: {}\n"
                    "Run all filesystem and shell work from this worktree path.",
                    worktree->path.string(),
                    worktree->branch,
                    worktree->head_commit);
            }
        } else {
            return ToolResult::error(std::format("Unsupported isolation mode '{}'", *plan->isolation));
        }
    }

    auto& store = loom::tools::agent_runtime::native_agent_store();
    struct ResumeLease {
        loom::tools::agent_runtime::NativeAgentStore& store;
        std::string id;
        bool held = false;
        ~ResumeLease() { if (held) store.release_resume(id); }
    } resume{store, plan->agent_id};
    if (input.host_agent_resume &&
        (!plan->resume_existing || !plan->background ||
         plan->agent_id != input.host_agent_resume->agent_id ||
         plan->agent_type != input.host_agent_resume->agent_type ||
         plan->parent_session_id != std::optional<std::string>{input.host_agent_resume->parent_session_id} ||
         (plan->team_name && !plan->team_name->empty()) || plan->teammate_backend || plan->teammate_task_id || plan->teammate_pane_id ||
         (plan->parent_agent_id && !plan->parent_agent_id->empty()))) {
        return ToolResult::error("Host continuation plan cannot redirect the agent target");
    }
    if (plan->resume_existing) {
        auto admitted = store.reserve_resume(plan->agent_id, input.host_agent_resume);
        if (!admitted) return ToolResult::error(admitted.error());
        resume.held = true;
    }

    if (plan->background) {
        return start_background_agent(std::move(*plan));
    }

    return run_agent_and_finalize(*plan, std::nullopt, input.is_cancelled);
}

int AgentTool::last_run_turns() const noexcept {
    return std::atomic_ref<int>(last_run_turns_).load(std::memory_order_relaxed);
}
int AgentTool::last_run_tokens() const noexcept {
    return std::atomic_ref<int>(last_run_tokens_).load(std::memory_order_relaxed);
}
void AgentTool::record_run_usage(int input_tokens, int output_tokens) noexcept {
    auto counter = std::atomic_ref<int>(last_run_tokens_);
    auto observed = counter.load(std::memory_order_relaxed);
    int next;
    do {
        const auto sum = static_cast<long long>(observed) + input_tokens + output_tokens;
        next = static_cast<int>(std::min(sum,
            static_cast<long long>(std::numeric_limits<int>::max())));
    } while (!counter.compare_exchange_weak(observed, next, std::memory_order_relaxed));
}


[[nodiscard]] Result<ToolResult> AgentTool::run_agent_and_finalize(
    const AgentExecutionPlan& plan,
    std::optional<std::string> startup_error,
    std::function<bool()> parent_is_cancelled) {
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
            // The parent callback is borrowed only during foreground execute().
            // Like ToolInput cancellation callbacks, it must not throw.
            // Forward it into the native cancellation source so blocked child
            // tools and model transfers observe the same cancellation signal.
            std::jthread parent_cancellation;
            if (!plan.background && parent_is_cancelled) {
                parent_cancellation = std::jthread(
                    [&store, id = plan.agent_id,
                     cancelled = std::move(parent_is_cancelled)](std::stop_token stop) {
                        while (!stop.stop_requested()) {
                            if (cancelled()) {
                                store.request_cancel(id, "Parent query cancelled");
                                return;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    });
            }
            result = startup_error ? ToolResult::error(*startup_error) : run_agent_loop(plan);
            if (parent_cancellation.joinable()) {
                parent_cancellation.request_stop();
                parent_cancellation.join();
            }
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
