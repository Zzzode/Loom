// Native execution, streaming and finalization for loom.orchestration.agent.
module;

module loom.orchestration.agent;

import std;
import loom.types.types;
import loom.services.api.errors;
import loom.services.compact.context_compaction;

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

    // Launch restrictions use the caller's team scope; instance identity only
    // inherits it for a named teammate or an exact existing roster member.
    std::optional<std::string> inherited_team_scope;
    if (!plan->resume_existing && !plan->team_name) {
        if (auto current_team = loom::utils::get_team_name();
            current_team && !current_team->empty()) {
            inherited_team_scope = *current_team;
            // Preserve explicit empty-name validation in start_teammate_agent.
            if (plan->name) {
                plan->team_name = std::move(*current_team);
            } else {
                const auto team = loom::tools::global_team_store().get_by_id_or_name(*current_team);
                if (team && std::ranges::any_of(team->members, [&](const auto& member) {
                        return member.agent_id == plan->agent_id;
                    }))
                    plan->team_name = std::move(*current_team);
            }
        }
    }

    if (current_session_is_teammate() && (plan->team_name || inherited_team_scope) && plan->name) {
        return ToolResult::error(
            "Teammates cannot spawn other teammates - the team roster is flat. "
            "To spawn a subagent instead, omit the name parameter.");
    }

    if (loom::utils::is_in_process_teammate() && (plan->team_name || inherited_team_scope) &&
        plan->background) {
        return ToolResult::error("In-process teammates cannot spawn background agents. "
                                 "Use run_in_background=false for synchronous subagents.");
    }

    if (!agent_type_allowed_by_permission_rules(plan->agent_type, config_.allowed_tools,
                                                config_.denied_tools)) {
        return ToolResult::error(
            std::format("Agent type '{}' is not allowed by current Agent tool permission rules",
                        plan->agent_type));
    }

    // Check recursion depth
    if (current_depth_ >= config_.max_depth) {
        return ToolResult::error(std::format("Agent recursion depth limit reached ({}/{})",
                                             current_depth_, config_.max_depth));
    }

    if (!plan->resume_existing && plan->team_name && plan->name && plan->isolation) {
        return ToolResult::error("teammate spawn does not support isolation; omit name/team_name "
                                 "to spawn an isolated subagent");
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
                    "user", loom::tools::agent_runtime::build_worktree_fork_notice(
                                parent_working_dir, worktree->path.string())));
            } else {
                plan->system_prompt +=
                    std::format("\n\nNative worktree isolation:\n"
                                "- worktree_path: {}\n"
                                "- worktree_branch: {}\n"
                                "- base_commit: {}\n"
                                "Run all filesystem and shell work from this worktree path.",
                                worktree->path.string(), worktree->branch, worktree->head_commit);
            }
        } else {
            return ToolResult::error(
                std::format("Unsupported isolation mode '{}'", *plan->isolation));
        }
    }

    auto& store = loom::tools::agent_runtime::native_agent_store();

    struct ResumeLease {
        loom::tools::agent_runtime::NativeAgentStore& store;
        std::string id;
        bool held = false;

        ~ResumeLease() {
            if (held) store.release_resume(id);
        }
    } resume{store, plan->agent_id};

    if (input.host_agent_resume &&
        (!plan->resume_existing || !plan->background ||
         plan->agent_id != input.host_agent_resume->agent_id ||
         plan->agent_type != input.host_agent_resume->agent_type ||
         plan->parent_session_id !=
             std::optional<std::string>{input.host_agent_resume->parent_session_id} ||
         plan->working_dir != input.host_agent_resume->cwd ||
         plan->team_name != input.host_agent_resume->team_name ||
         plan->name != input.host_agent_resume->name ||
         plan->teammate_backend != input.host_agent_resume->teammate_backend ||
         plan->teammate_task_id != input.host_agent_resume->teammate_task_id ||
         plan->teammate_pane_id != input.host_agent_resume->teammate_pane_id ||
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

Result<ToolResult> AgentTool::start_background_agent(AgentExecutionPlan plan) {
    // This entrypoint selects a native worker, not a pane backend. Only a
    // fresh, registered roster member gains this capability provenance.
    // Retained unknown/external identities are never inferred or upgraded.
    if (registry_ && !plan.resume_existing && plan.team_name && !plan.team_name->empty() &&
        !plan.teammate_backend && !plan.teammate_task_id && !plan.teammate_pane_id) {
        const auto team = loom::tools::global_team_store().get_by_id_or_name(*plan.team_name);
        if (team && std::ranges::any_of(team->members, [&](const auto& member) {
                return member.agent_id == plan.agent_id;
            }))
            plan.teammate_backend = "in-process";
    }
    loom::tools::MessageRouter::instance().register_agent(plan.agent_id);
    upsert_agent_record_for_plan(plan);

    auto start_hooks = execute_agent_frontmatter_hooks(plan, "SubagentStart");
    if (!start_hooks.ok()) {
        return run_agent_and_finalize(plan, *start_hooks.error);
    }
    if (!start_hooks.output.empty()) {
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            plan.agent_id, "hook SubagentStart: " + start_hooks.output);
    }
    plan.hook_additional_contexts = std::move(start_hooks.additional_contexts);

    if (!registry_) {
        if (!plan.fork_context_includes_prompt) {
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "user: " + plan.prompt);
        }
        return ToolResult::success(std::format(
            "Queued background agent {} ({}) but execution is deferred because no tool registry is "
            "attached.\nagentId: {}\noutputFile: {}",
            plan.agent_id, plan.agent_type, plan.agent_id, agent_output_file_path(plan.agent_id)));
    }

    auto config = config_;
    auto depth = current_depth_;
    // Registry shutdown cancels and drains all workers borrowing its tools
    // and permission callback before either dependency can be destroyed.
    auto* registry = registry_;
    auto permission_check =
        permission_hook_valid_for_background_ ? permission_check_ : AgentLivePermissionCheckFn{};
    auto permission_hook_valid_for_background = permission_hook_valid_for_background_;
    auto agent_id = plan.agent_id;
    try {
        loom::tools::agent_runtime::native_agent_store().run_background_agent(
            agent_id,
            [plan, config = std::move(config), depth, registry,
             permission_check = std::move(permission_check),
             permission_hook_valid_for_background]() mutable {
                AgentTool worker(std::move(config), depth, registry, std::move(permission_check),
                                 permission_hook_valid_for_background);
                (void)worker.run_agent_and_finalize(plan);
            },
            registry->background_stop_token());
    } catch (const std::exception& error) {
        return run_agent_and_finalize(plan, std::string(error.what()));
    }

    return ToolResult::success(
        std::format("Started background agent {} ({})\nagentId: {}\noutputFile: {}", plan.agent_id,
                    plan.agent_type, plan.agent_id, agent_output_file_path(plan.agent_id)));
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
        next = static_cast<int>(
            std::min(sum, static_cast<long long>(std::numeric_limits<int>::max())));
    } while (!counter.compare_exchange_weak(observed, next, std::memory_order_relaxed));
}

[[nodiscard]] Result<ToolResult>
AgentTool::run_agent_and_finalize(const AgentExecutionPlan& plan,
                                  std::optional<std::string> startup_error,
                                  std::function<bool()> parent_is_cancelled) {
    auto& store = loom::tools::agent_runtime::native_agent_store();

    struct ExecutionGuard {
        loom::tools::agent_runtime::NativeAgentStore& store;
        const std::string& id;
        bool active = false;

        ~ExecutionGuard() {
            if (active) store.end_execution(id);
        }
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
                upsert_agent_record_for_plan(plan, registry_ ? registry_->background_stop_token()
                                                             : std::stop_token{});
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
    store.close_input(plan.agent_id);
    store.clear_live_text(plan.agent_id);
    (void)cleanup_agent_worktree(plan.agent_id);
    const bool succeeded = result && !result->is_error;
    auto text = result ? tool_result_content_text(*result) : result.error().format();
    if (succeeded)
        store.finish_agent(plan.agent_id, text);
    else
        store.finish_agent(plan.agent_id, std::unexpected(text));
    // Team status projects the stable native terminal state. A cancellation
    // racing finalization must not leave a cancelled agent marked Done.
    const auto record = store.get(plan.agent_id);
    const bool completed =
        record && record->status == loom::tools::agent_runtime::NativeAgentStatus::Completed;
    update_teammate_completion_status(
        plan, completed,
        record ? (completed ? record->output.value_or(text) : record->error.value_or(text)) : text);
    if (record && record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled) {
        return ToolResult::error(record->error.value_or("Agent cancelled"));
    }
    return result;
}

void AgentTool::append_queued_agent_messages(const AgentExecutionPlan& plan,
                                             std::vector<Message>& messages,
                                             std::optional<std::size_t>& request_anchor) const {
    for (auto& pending_message :
         loom::tools::agent_runtime::native_agent_store().take_pending_messages(plan.agent_id)) {
        auto queued_message = Message::from_text("user", pending_message);
        request_anchor = messages.size();
        append_agent_sidechain_message(plan.agent_id, queued_message, std::nullopt, true);
        messages.push_back(std::move(queued_message));
    }
    SendMessageTool inbox(plan.agent_id);
    while (auto message = inbox.receive()) {
        auto queued_message = Message::from_text(
            "user", std::format("[Message from {} priority={}]\n{}", message->from_agent,
                                message_priority_name(message->priority), message->content));
        request_anchor = messages.size();
        append_agent_sidechain_message(plan.agent_id, queued_message, std::nullopt, true);
        messages.push_back(std::move(queued_message));
    }
}

Result<ToolResult> AgentTool::run_agent_loop(const AgentExecutionPlan& plan) {
    auto resumed_record = plan.resume_existing
                              ? loom::tools::agent_runtime::native_agent_store().get(plan.agent_id)
                              : std::optional<loom::tools::agent_runtime::NativeAgentRecord>{};

    std::string final_output;
    bool agent_completed = false;
    bool agent_hook_prevented_continuation = false;
    std::optional<std::string> agent_hook_stop_reason;
    bool lifecycle_hooks_active = plan.background;
    bool stop_hooks_executed = false;
    auto run_stop_hooks_once =
        [&](std::string_view last_assistant_message) -> std::optional<std::string> {
        if (!lifecycle_hooks_active || stop_hooks_executed) return std::nullopt;
        stop_hooks_executed = true;
        auto stop_hooks =
            execute_agent_frontmatter_hooks(plan, "SubagentStop", last_assistant_message);
        if (!stop_hooks.output.empty()) {
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "hook SubagentStop: " + stop_hooks.output);
        }
        if (!stop_hooks.ok()) {
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "hook SubagentStop failed: " + *stop_hooks.error);
            return *stop_hooks.error;
        }
        return std::nullopt;
    };

    auto fail_agent = [&](std::string error) -> Result<ToolResult> {
        if (auto hook_error = run_stop_hooks_once(error)) {
            error += "\nSubagentStop hook failed: " + *hook_error;
        }
        return ToolResult::error(std::move(error));
    };

    loom::tools::agent_runtime::native_agent_store().mark_running(plan.agent_id);
    auto hook_additional_contexts = plan.hook_additional_contexts;
    if (!plan.background) {
        auto start_hooks = execute_agent_frontmatter_hooks(plan, "SubagentStart");
        if (!start_hooks.ok()) return fail_agent(*start_hooks.error);
        if (!start_hooks.output.empty()) {
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "hook SubagentStart: " + start_hooks.output);
        }
        hook_additional_contexts.insert(
            hook_additional_contexts.end(),
            std::make_move_iterator(start_hooks.additional_contexts.begin()),
            std::make_move_iterator(start_hooks.additional_contexts.end()));
        lifecycle_hooks_active = true;
    }

    MessagesClient client(utils::agent_api_client_config(plan.api_context));

    // Restore the committed working snapshot before adding this run's input.
    auto replay = resumed_record ? utils::replay_agent_context(resumed_record->sidechain_entries)
                                 : utils::AgentReplayContext{};
    const bool resumed_history = !replay.messages.empty();
    std::vector<Message> messages;
    std::optional<std::size_t> request_anchor;
    if (!replay.messages.empty()) {
        std::optional<std::string> anchor_content;
        if (replay.request_anchor && *replay.request_anchor < replay.messages.size())
            anchor_content = utils::message_json_object(replay.messages[*replay.request_anchor]);
        messages = resume_messages_from_sidechain_entries(resumed_record->sidechain_entries);
        if (anchor_content) {
            for (std::size_t index = 0; index < messages.size(); ++index)
                if (utils::message_json_object(messages[index]) == *anchor_content)
                    request_anchor = index;
        }
    } else {
        for (const auto& context_message : plan.fork_context_messages) {
            const bool directive =
                plan.fork_request_anchor &&
                *plan.fork_request_anchor < plan.fork_context_messages.size() &&
                &context_message == &plan.fork_context_messages[*plan.fork_request_anchor];
            if (directive) request_anchor = messages.size();
            append_agent_sidechain_message(plan.agent_id, context_message, std::nullopt, directive);
            messages.push_back(context_message);
        }
        for (const auto& skill_message : plan.preloaded_skill_messages) {
            auto message = Message::from_text("user", skill_message);
            append_agent_sidechain_message(plan.agent_id, message);
            messages.push_back(std::move(message));
        }
        if (resumed_record && !resumed_record->transcript.empty()) {
            auto message =
                Message::from_text("user", format_resumed_agent_context(*resumed_record));
            append_agent_sidechain_message(plan.agent_id, message);
            messages.push_back(std::move(message));
        }
    }
    const auto hook_message_start = messages.size();
    append_hook_additional_context_messages(messages, hook_additional_contexts);
    for (std::size_t i = hook_message_start; i < messages.size(); ++i)
        append_agent_sidechain_message(plan.agent_id, messages[i]);
    if (plan.agent_mcp_context_message && !resumed_history) {
        auto message = Message::from_text("user", *plan.agent_mcp_context_message);
        append_agent_sidechain_message(plan.agent_id, message);
        messages.push_back(std::move(message));
    }
    append_queued_agent_messages(plan, messages, request_anchor);
    if (plan.resume_existing || !plan.fork_context_includes_prompt || !request_anchor) {
        auto prompt_message = Message::from_text("user", plan.prompt);
        // Publish the canonical message and its diagnostic projection together.
        // Observers must never materialize a fallback between the two writes.
        request_anchor = messages.size();
        append_agent_sidechain_message(plan.agent_id, prompt_message, "user: " + plan.prompt, true);
        messages.push_back(std::move(prompt_message));
    }
    if (plan.critical_system_reminder && !plan.system_prompt_overridden) {
        auto message = Message::from_text(
            "user", format_critical_system_reminder(*plan.critical_system_reminder));
        append_agent_sidechain_message(plan.agent_id, message);
        messages.push_back(std::move(message));
    }

    auto content_replacement_state =
        resumed_record
            ? agent_content_replacement_state_from_entries(resumed_record->sidechain_entries)
            : AgentContentReplacementState{};
    mark_seen_tool_result_ids(content_replacement_state, messages);
    const auto content_replacement_skip_tools =
        registry_ ? unbounded_tool_result_budget_names(registry_->get_visible_definitions())
                  : std::unordered_set<std::string>{};
    const auto content_replacement_thresholds =
        registry_ ? tool_result_budget_thresholds(registry_->get_visible_definitions())
                  : std::unordered_map<std::string, std::size_t>{};

    auto cancel_if_requested = [&](std::string_view phase) -> std::optional<std::string> {
        if (!loom::tools::agent_runtime::native_agent_store().is_cancel_requested(plan.agent_id)) {
            return std::nullopt;
        }
        const auto reason = std::format("Agent {} cancelled {}", plan.agent_id, phase);
        (void)run_stop_hooks_once(reason);
        return reason;
    };
    auto update_turn_progress = [&](int turn, double phase) {
        const auto denominator = static_cast<double>(std::max(plan.max_turns, 1));
        const auto progress = std::min(0.99, (static_cast<double>(turn) + phase) / denominator);
        loom::tools::agent_runtime::native_agent_store().update_progress(plan.agent_id, progress);
    };

    auto& input_store = loom::tools::agent_runtime::native_agent_store();

    struct InputGuard {
        loom::tools::agent_runtime::NativeAgentStore& store;
        const std::string& id;

        ~InputGuard() {
            store.close_input(id);
            store.clear_live_text(id);
        }
    } input_guard{input_store, plan.agent_id};

    (void)input_store.open_input(plan.agent_id);

    loom::services::compact::UsageObservation observed;
    for (int turn = 0; turn < plan.max_turns; ++turn) {
        if (auto reason = cancel_if_requested(std::format("before turn {}", turn + 1))) {
            return ToolResult::error(std::move(*reason));
        }
        update_turn_progress(turn, 0.0);
        append_queued_agent_messages(plan, messages, request_anchor);
        const auto budget_outcome = apply_agent_tool_result_budget(
            plan.agent_id, messages, content_replacement_state, content_replacement_skip_tools,
            content_replacement_thresholds);
        if (budget_outcome.newly_replaced) observed = {};

        CreateMessageRequest req;
        // Consume the stream and accumulate content blocks
        std::string text_content;
        std::vector<ContentBlock> tool_uses;
        std::string stop_reason;
        ContentBlock current_block;
        std::string accumulated_json;
        bool in_block = false;
        bool message_stopped = false;
        int input_tokens = 0;
        int output_tokens = 0;
        int cache_creation_tokens = 0;
        int cache_read_tokens = 0;

        for (int attempt = 0;; ++attempt) {
            // Build request
            req = {};
            req.model = plan.model;
            req.messages = messages;
            const auto context_policy = utils::agent_context_policy(plan.api_context, plan.model);
            req.max_tokens =
                static_cast<int>(utils::agent_model_context(plan.api_context, plan.model)
                                     .request_output_tokens.value_or(0));
            req.stream = true;
            if (!plan.system_prompt.empty()) req.system_prompt = plan.system_prompt;
            req.tools = api_tools_for_plan(plan);
            apply_agent_effort_to_request(req, plan.effort);

            const auto cancelled = [&] {
                return loom::tools::agent_runtime::native_agent_store().is_cancel_requested(
                    plan.agent_id);
            };
            const auto compact_history = [&](std::string_view reason, bool payload = false) {
                return utils::compact_agent_context(
                    client, req, plan.api_context, plan.agent_id, messages, request_anchor,
                    observed, reason,
                    [&](int input, int output) { record_run_usage(input, output); }, cancelled,
                    payload);
            };
            if (attempt == 0 && (!plan.api_context || plan.api_context->context_auto_compact) &&
                utils::estimate_agent_context(req, request_anchor, observed) >=
                    loom::services::compact::usable_input_tokens(context_policy)) {
                auto compacted = compact_history("budget");
                if (!compacted)
                    return fail_agent("Agent context compaction failed: " +
                                      compacted.error().message);
                req.messages = messages;
            }
            const auto request_estimate = utils::estimate_agent_context(req, request_anchor);
            // A summary and overflow retry do not consume an additional business turn.
            if (attempt == 0)
                std::atomic_ref<int>(last_run_turns_).fetch_add(1, std::memory_order_relaxed);
            auto stream_result = utils::create_agent_model_stream(client, req, plan.api_context);
            if (!stream_result) {
                return fail_agent(
                    std::format("Agent API call failed: {}", stream_result.error().message()));
            }
            update_turn_progress(turn, 0.05);

            auto& parser = *stream_result;

            text_content.clear();
            tool_uses.clear();
            stop_reason.clear();
            current_block = {};
            accumulated_json.clear();
            in_block = false;
            message_stopped = false;
            input_tokens = output_tokens = cache_creation_tokens = cache_read_tokens = 0;
            std::optional<loom::services::api::errors::ApiErrorDetails> response_error;
            while (true) {
                if (auto reason = cancel_if_requested("while waiting for model stream")) {
                    return ToolResult::error(std::move(*reason));
                }
                auto event_result = parser.next_event();
                if (event_result && !event_result->has_value()) {
                    if (!parser.is_finished()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    // Terminal publication stabilizes the queue; drain a late final frame.
                    event_result = parser.next_event();
                }
                if (!event_result) {
                    response_error.emplace();
                    response_error->error_message = event_result.error().message();
                    break;
                }
                if (!event_result->has_value()) {
                    if (parser.has_error()) {
                        response_error = parser.error_details().value_or(
                            loom::services::api::errors::ApiErrorDetails{});
                        if (response_error->error_message.empty())
                            response_error->error_message = "stream connection failed";
                        break;
                    }
                    break;
                }

                const auto& event = **event_result;

                switch (event.type) {
                case StreamEventType::MessageStart:
                    if (event.input_tokens) input_tokens = std::max(0, *event.input_tokens);
                    if (event.cache_creation_tokens)
                        cache_creation_tokens = std::max(0, *event.cache_creation_tokens);
                    if (event.cache_read_tokens)
                        cache_read_tokens = std::max(0, *event.cache_read_tokens);
                    update_turn_progress(turn, 0.10);
                    break;

                case StreamEventType::ContentBlockStart:
                    update_turn_progress(turn, 0.20);
                    in_block = true;
                    current_block = ContentBlock{};
                    if (event.block_type == StreamContentBlockType::Text) {
                        current_block.type = ContentBlockType::Text;
                        current_block.text = event.block_text;
                        input_store.append_live_text(plan.agent_id, event.block_text);
                    } else if (event.block_type == StreamContentBlockType::ToolUse) {
                        current_block.type = ContentBlockType::ToolUse;
                        current_block.tool_use_id = event.tool_use_id;
                        current_block.tool_name = event.tool_name;
                        accumulated_json.clear();
                    }
                    break;

                case StreamEventType::ContentBlockDelta:
                    update_turn_progress(turn, 0.35);
                    if (event.delta.type == StreamContentBlockType::Text) {
                        current_block.text += event.delta.text;
                        input_store.append_live_text(plan.agent_id, event.delta.text);
                    } else if (event.delta.type == StreamContentBlockType::ToolUse) {
                        accumulated_json += event.delta.partial_json;
                    }
                    break;

                case StreamEventType::ContentBlockStop:
                    update_turn_progress(turn, 0.45);
                    if (in_block) {
                        if (current_block.type == ContentBlockType::Text) {
                            text_content += current_block.text;
                        } else if (current_block.type == ContentBlockType::ToolUse) {
                            current_block.tool_input_json = accumulated_json;
                            tool_uses.push_back(current_block);
                        }
                        in_block = false;
                    }
                    break;

                case StreamEventType::MessageDelta:
                    if (event.message_delta.usage.input_tokens) {
                        input_tokens = std::max(0, *event.message_delta.usage.input_tokens);
                    }
                    if (event.message_delta.usage.output_tokens) {
                        output_tokens = std::max(0, *event.message_delta.usage.output_tokens);
                    }
                    if (event.message_delta.usage.cache_creation_tokens)
                        cache_creation_tokens =
                            std::max(0, *event.message_delta.usage.cache_creation_tokens);
                    if (event.message_delta.usage.cache_read_tokens)
                        cache_read_tokens =
                            std::max(0, *event.message_delta.usage.cache_read_tokens);
                    update_turn_progress(turn, 0.55);
                    if (event.message_delta.stop_reason) {
                        stop_reason = *event.message_delta.stop_reason;
                    }
                    break;

                case StreamEventType::MessageStop:
                    update_turn_progress(turn, 0.65);
                    // Wait for HTTP completion as well: an error status or
                    // a failed transfer can arrive after the final frame.
                    message_stopped = true;
                    break;

                case StreamEventType::Error:
                    response_error = event.error;
                    break;

                default:
                    break;
                }
                if (response_error) break;
            }
            const auto full_input = static_cast<int>(std::min<long long>(
                static_cast<long long>(input_tokens) + cache_creation_tokens + cache_read_tokens,
                std::numeric_limits<int>::max()));
            record_run_usage(full_input, output_tokens);
            if (response_error) {
                // Stop and join any remaining producer before summary generation.
                parser = loom::services::api::StreamParser{};
                if (attempt == 0 && utils::agent_context_failure(*response_error) !=
                                        loom::services::compact::ContextFailure::None) {
                    loom::tools::agent_runtime::native_agent_store().clear_live_text(plan.agent_id);
                    auto compacted = compact_history(
                        "overflow",
                        utils::agent_context_failure(*response_error) ==
                            loom::services::compact::ContextFailure::RequestBytesExceeded);
                    if (!compacted)
                        return fail_agent("Agent context recovery failed: " +
                                          compacted.error().message);
                    continue;
                }
                return fail_agent("Agent stream error: " +
                                  (response_error->http_status > 0
                                       ? std::format("HTTP {}: ", response_error->http_status)
                                       : std::string{}) +
                                  response_error->error_message);
            }
            observed = {static_cast<std::uint64_t>(full_input), request_estimate};
            if (!message_stopped || in_block) {
                return fail_agent("Agent stream ended before the message completed");
            }
            break;
        }  // One business request, with at most one overflow recovery.

        // Completion must be explicit and leave no pending tool calls.
        if (stop_reason == "end_turn" && tool_uses.empty()) {
            final_output = text_content;
            if (!text_content.empty()) {
                auto assistant_message = Message::from_text("assistant", text_content);
                append_agent_sidechain_message(plan.agent_id, assistant_message);
                loom::tools::agent_runtime::native_agent_store().append_transcript(
                    plan.agent_id, "assistant: " + text_content);
            }
            // A send and the end-turn decision share the same Store lock.
            // Accepted late input stays pending on a budget exit; it is never lost.
            if (!input_store.close_input_if_idle(plan.agent_id)) {
                messages.push_back(Message::from_text("assistant", text_content));
                continue;
            }
            agent_completed = true;
            break;
        }

        // Add assistant message to history
        Message assistant_msg;
        assistant_msg.role = "assistant";
        if (!text_content.empty()) {
            assistant_msg.content.push_back(
                ContentBlock{.type = ContentBlockType::Text, .text = text_content});
        }
        for (auto& tu : tool_uses) {
            assistant_msg.content.push_back(tu);
        }
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            plan.agent_id, "assistant: " + message_content_text(assistant_msg));
        append_agent_sidechain_message(plan.agent_id, assistant_msg);
        messages.push_back(std::move(assistant_msg));
        if (stop_reason != "tool_use" || tool_uses.empty()) {
            if (!text_content.empty()) final_output += text_content + "\n";
            auto error = std::format(
                "Agent {} stopped before completing the task: stop_reason={}. "
                "Resume the agent to continue; check the endpoint response or output-token limit.",
                plan.agent_id, stop_reason.empty() ? "[missing]" : stop_reason);
            if (!final_output.empty()) error += "\n\nPartial output:\n" + final_output;
            return fail_agent(std::move(error));
        }
        const auto& parent_assistant_message = messages.back();
        update_turn_progress(turn, 0.70);

        // Execute tools and add results
        Message tool_result_msg;
        tool_result_msg.role = "user";
        auto finish_cancelled_tools =
            [&](std::string reason, std::string_view interrupted_id = {}) -> Result<ToolResult> {
            // Preserve completed calls and close every emitted tool use.
            // Otherwise resume filtering drops the assistant call and its
            // execution/cancellation history, inviting duplicate effects.
            for (const auto& pending : tool_uses) {
                if (std::ranges::any_of(tool_result_msg.content, [&](const auto& result) {
                        return result.tool_use_id == pending.tool_use_id;
                    }))
                    continue;
                ContentBlock cancelled;
                cancelled.type = ContentBlockType::ToolResult;
                cancelled.tool_use_id = pending.tool_use_id;
                cancelled.is_error = true;
                cancelled.text = pending.tool_use_id == interrupted_id
                                     ? std::format("[Tool cancelled during execution: {}. Tool "
                                                   "side effects may have occurred; "
                                                   "do not retry automatically.]",
                                                   reason)
                                     : "[Tool not executed: agent cancelled before this call.]";
                tool_result_msg.content.push_back(std::move(cancelled));
            }
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "user: " + message_content_text(tool_result_msg));
            append_agent_sidechain_message(plan.agent_id, tool_result_msg);
            messages.push_back(std::move(tool_result_msg));
            return ToolResult::error(std::move(reason));
        };
        for (const auto& tu : tool_uses) {
            if (auto reason = cancel_if_requested("before executing tool " + tu.tool_name)) {
                return finish_cancelled_tools(std::move(*reason));
            }
            update_turn_progress(turn, 0.75);
            ContentBlock result_block;
            result_block.type = ContentBlockType::ToolResult;
            result_block.tool_use_id = tu.tool_use_id;
            auto tool_input_json = tu.tool_input_json;
            bool post_tool_hooks_enabled = false;
            bool tool_execution_succeeded = true;
            bool pre_tool_contexts_appended = false;
            bool tool_hook_prevented_continuation = false;
            std::optional<std::string> tool_hook_stop_reason;
            std::vector<std::string> pre_tool_additional_contexts;

            auto append_tool_hook_context = [&](std::string_view event,
                                                const std::vector<std::string>& contexts) {
                if (contexts.empty()) return;
                if (!result_block.text.empty()) result_block.text += "\n\n";
                result_block.text += format_tool_hook_additional_context(event, tu.tool_name,
                                                                         tu.tool_use_id, contexts);
            };
            auto append_pre_tool_context_once = [&] {
                if (pre_tool_contexts_appended) return;
                pre_tool_contexts_appended = true;
                append_tool_hook_context("PreToolUse", pre_tool_additional_contexts);
            };
            auto record_hook_prevented_continuation = [&](const AgentHookExecutionResult& hooks,
                                                          std::string_view fallback_reason) {
                if (!hooks.prevent_continuation) return;
                tool_hook_prevented_continuation = true;
                tool_hook_stop_reason = hooks.stop_reason.value_or(std::string(fallback_reason));
            };

            if (!registry_) {
                result_block.text = "[Tool execution not available: no registry]";
                tool_execution_succeeded = false;
            } else if (should_reject_fork_child_agent_call(plan, tu.tool_name,
                                                           tu.tool_input_json)) {
                result_block.text = "[Tool execution error: Fork is not available inside a forked "
                                    "worker. Complete your task directly using your tools.]";
                tool_execution_succeeded = false;
            } else if (!is_tool_allowed_for_plan(tu.tool_name, plan)) {
                result_block.text =
                    std::format("[Tool '{}' not available in sub-agent context]", tu.tool_name);
                tool_execution_succeeded = false;
            } else if (tu.tool_name == "Agent" && current_depth_ + 1 >= config_.max_depth) {
                result_block.text = std::format("[Agent recursion depth limit reached ({}/{})]",
                                                current_depth_ + 1, config_.max_depth);
                tool_execution_succeeded = false;
            } else {
                tool_input_json = apply_agent_tool_execution_context_to_input(
                    tu.tool_name, tu.tool_input_json, plan);
                if (tu.tool_name == "Agent" && agent_tool_input_omits_agent_type(tool_input_json)) {
                    tool_input_json = build_implicit_fork_agent_input_json(
                        tool_input_json, plan, parent_assistant_message, req.tools);
                }

                auto pre_hooks = execute_agent_tool_frontmatter_hooks(
                    plan, "PreToolUse", tu.tool_name, tool_input_json, tu.tool_use_id);
                if (!pre_hooks.output.empty()) {
                    loom::tools::agent_runtime::native_agent_store().append_transcript(
                        plan.agent_id,
                        std::format("hook PreToolUse:{}: {}", tu.tool_name, pre_hooks.output));
                }
                if (!pre_hooks.ok()) {
                    result_block.text = std::format(
                        "[Tool execution denied by PreToolUse hook: {}]", *pre_hooks.error);
                    tool_execution_succeeded = false;
                    record_hook_prevented_continuation(pre_hooks,
                                                       "Execution stopped by PreToolUse hook");
                    pre_tool_additional_contexts = std::move(pre_hooks.additional_contexts);
                    append_pre_tool_context_once();
                } else {
                    record_hook_prevented_continuation(pre_hooks,
                                                       "Execution stopped by PreToolUse hook");
                    pre_tool_additional_contexts = std::move(pre_hooks.additional_contexts);
                    if (pre_hooks.updated_input_json) {
                        tool_input_json = *pre_hooks.updated_input_json;
                        tool_input_json = apply_agent_tool_execution_context_to_input(
                            tu.tool_name, tool_input_json, plan);
                        loom::tools::agent_runtime::native_agent_store().append_transcript(
                            plan.agent_id, std::format("hook PreToolUse:{} updated input: {}",
                                                       tu.tool_name, tool_input_json));
                    }
                    bool skip_tool_execution = false;
                    auto live_permission =
                        check_live_tool_permission(tu.tool_name, tool_input_json, tu.tool_use_id);
                    if (!live_permission.allowed) {
                        auto reason = live_permission.message.value_or(
                            std::format("Permission denied for tool: {}", tu.tool_name));
                        result_block.text =
                            std::format("[Tool execution denied by permission hook: {}]", reason);
                        tool_execution_succeeded = false;
                        post_tool_hooks_enabled = true;
                        skip_tool_execution = true;
                    } else if (live_permission.updated_input_json) {
                        tool_input_json = *live_permission.updated_input_json;
                        tool_input_json = apply_agent_tool_execution_context_to_input(
                            tu.tool_name, tool_input_json, plan);
                        loom::tools::agent_runtime::native_agent_store().append_transcript(
                            plan.agent_id, std::format("permission hook {} updated input: {}",
                                                       tu.tool_name, tool_input_json));
                    }
                    if (!skip_tool_execution) {
                        auto tool_input = ToolInput::from_json(tool_input_json);
                        tool_input.parent_model = plan.model;
                        tool_input.api_context = plan.api_context;
                        tool_input.parent_session_id = plan.parent_session_id;
                        tool_input.is_cancelled = [id = plan.agent_id] {
                            return loom::tools::agent_runtime::native_agent_store()
                                .is_cancel_requested(id);
                        };
                        if (auto scope_error = mcp_scope_error_for_plan(tool_input, plan)) {
                            result_block.text =
                                std::format("[Tool execution error: {}]", *scope_error);
                            tool_execution_succeeded = false;
                            post_tool_hooks_enabled = true;
                            skip_tool_execution = true;
                        }
                        if (!skip_tool_execution) {
                            post_tool_hooks_enabled = true;
                            if (normalized_tool_name_is(tu.tool_name, "sleep")) {
                                auto exec_result =
                                    execute_agent_sleep_tool(plan.agent_id, tool_input);
                                if (auto reason =
                                        cancel_if_requested("while executing tool sleep")) {
                                    return finish_cancelled_tools(std::move(*reason),
                                                                  tu.tool_use_id);
                                }
                                tool_execution_succeeded = !exec_result.is_error;
                                std::string output;
                                for (const auto& c : exec_result.content) {
                                    if (!output.empty()) output += "\n";
                                    output += c.text;
                                }
                                result_block.text = std::move(output);
                            } else if (normalized_tool_name_is(tu.tool_name, "WebFetch")) {
                                auto exec_result =
                                    execute_agent_web_fetch_tool(plan.agent_id, tool_input);
                                if (auto reason =
                                        cancel_if_requested("while executing tool WebFetch")) {
                                    return finish_cancelled_tools(std::move(*reason),
                                                                  tu.tool_use_id);
                                }
                                tool_execution_succeeded = !exec_result.is_error;
                                std::string output;
                                for (const auto& c : exec_result.content) {
                                    if (!output.empty()) output += "\n";
                                    output += c.text;
                                }
                                result_block.text = std::move(output);
                            } else if (tu.tool_name == "Agent") {
                                AgentTool child(child_config(plan.agent_id, plan.mode, plan.model),
                                                current_depth_ + 1, registry_, permission_check_,
                                                permission_hook_valid_for_background_);
                                auto child_result = child.execute(tool_input);
                                if (child_result) {
                                    tool_execution_succeeded = !child_result->is_error;
                                    std::string output;
                                    for (const auto& c : child_result->content) {
                                        if (!output.empty()) output += "\n";
                                        output += c.text;
                                    }
                                    result_block.text = std::move(output);
                                } else {
                                    tool_execution_succeeded = false;
                                    result_block.text = std::format("[Tool execution error: {}]",
                                                                    child_result.error().format());
                                }
                            } else {
                                auto exec_result = registry_->execute(tu.tool_name, tool_input);
                                if (auto reason = cancel_if_requested("while executing tool " +
                                                                      tu.tool_name)) {
                                    return finish_cancelled_tools(std::move(*reason),
                                                                  tu.tool_use_id);
                                }
                                if (exec_result) {
                                    tool_execution_succeeded = !exec_result->is_error;
                                    // Concatenate all content blocks from the tool result
                                    std::string output;
                                    for (const auto& c : exec_result->content) {
                                        if (!output.empty()) output += "\n";
                                        output += c.text;
                                    }
                                    result_block.text = std::move(output);
                                } else {
                                    tool_execution_succeeded = false;
                                    result_block.text = std::format("[Tool execution error: {}]",
                                                                    exec_result.error().message);
                                }
                            }
                        }
                    }
                }
            }

            if (post_tool_hooks_enabled) {
                const auto hook_event = tool_execution_succeeded
                                            ? std::string_view{"PostToolUse"}
                                            : std::string_view{"PostToolUseFailure"};
                const auto output_preview = tool_execution_succeeded
                                                ? agent_hook_output_preview(result_block.text)
                                                : std::string{};
                const auto error_preview = tool_execution_succeeded
                                               ? std::string{}
                                               : agent_hook_output_preview(result_block.text);
                auto post_hooks = execute_agent_tool_frontmatter_hooks(
                    plan, hook_event, tu.tool_name, tool_input_json, tu.tool_use_id, output_preview,
                    error_preview);
                if (!post_hooks.output.empty()) {
                    loom::tools::agent_runtime::native_agent_store().append_transcript(
                        plan.agent_id,
                        std::format("hook {}:{}: {}", hook_event, tu.tool_name, post_hooks.output));
                }
                if (!post_hooks.ok()) {
                    if (!result_block.text.empty()) result_block.text += "\n\n";
                    result_block.text +=
                        std::format("[{} hook error: {}]", hook_event, *post_hooks.error);
                }
                if (hook_event == "PostToolUse") {
                    if (post_hooks.ok() && is_mcp_tool_name(tu.tool_name) &&
                        post_hooks.updated_mcp_tool_output_text) {
                        result_block.text = *post_hooks.updated_mcp_tool_output_text;
                        loom::tools::agent_runtime::native_agent_store().append_transcript(
                            plan.agent_id,
                            std::format("hook PostToolUse:{} updated MCP output: {}", tu.tool_name,
                                        agent_hook_output_preview(result_block.text)));
                    }
                    record_hook_prevented_continuation(post_hooks,
                                                       "Execution stopped by PostToolUse hook");
                }
                append_pre_tool_context_once();
                append_tool_hook_context(hook_event, post_hooks.additional_contexts);
            }
            append_pre_tool_context_once();
            if (tool_hook_prevented_continuation) {
                if (!result_block.text.empty()) result_block.text += "\n\n";
                result_block.text +=
                    std::format("[Hook stopped continuation: {}]",
                                tool_hook_stop_reason.value_or("Execution stopped by hook"));
            }

            result_block.is_error = !tool_execution_succeeded;
            tool_result_msg.content.push_back(std::move(result_block));
            if (tool_hook_prevented_continuation) {
                agent_hook_prevented_continuation = true;
                agent_hook_stop_reason = std::move(tool_hook_stop_reason);
            }
            update_turn_progress(turn, 0.85);
        }
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            plan.agent_id, "user: " + message_content_text(tool_result_msg));
        if (plan.critical_system_reminder) {
            tool_result_msg.content.push_back(ContentBlock{
                .type = ContentBlockType::Text,
                .text = format_critical_system_reminder(*plan.critical_system_reminder)});
        }
        append_agent_sidechain_message(plan.agent_id, tool_result_msg);
        messages.push_back(std::move(tool_result_msg));
        update_turn_progress(turn, 0.90);

        // Record partial output
        if (!text_content.empty()) {
            final_output += text_content + "\n";
        }
        if (agent_hook_prevented_continuation) {
            agent_completed = true;
            const auto stop_text = agent_hook_stop_reason.value_or("Execution stopped by hook");
            if (!final_output.empty() && !final_output.ends_with('\n')) final_output += "\n";
            final_output += stop_text;
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                plan.agent_id, "hook stopped continuation: " + stop_text);
            break;
        }
    }

    if (!agent_completed) {
        auto error =
            std::format("Agent {} reached its maximum of {} turns before completing the task. "
                        "Resume the agent to continue, or increase maxTurns in its definition.",
                        plan.agent_id, plan.max_turns);
        if (!final_output.empty()) error += "\n\nPartial output:\n" + final_output;
        return fail_agent(std::move(error));
    }

    if (final_output.empty()) {
        final_output = "[Agent completed without producing output]";
    }

    if (auto hook_error = run_stop_hooks_once(final_output)) return fail_agent(*hook_error);

    return ToolResult::success(final_output);
}

}  // namespace loom::tools::agent
