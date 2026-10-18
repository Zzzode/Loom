// app_agent_menu.cpp — impl unit for AppAdapter methods kept OUT of
// app_autocomplete.cpp to stay under clang's 2GB source-location budget.
//
// Contains: FormatAgentsMenuOutput, LoadAgentCardsForMenu, SyncState,
//           ConsumePendingResult, DrainCompletedPastesAndSubmit,
//           get_permission_callback, trigger_orphan_cleanup_for_testing.
//
// Splitting these out removes agent_display + agent_cards + dialogs.triggers
// + dialogs.system imports from app_autocomplete.cpp.
module;

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ftxui/component/event.hpp>

module loom.ui.app.app;

import std;
import loom.query.query_engine;
import loom.commands.command;

// ── Base imports (shared with app_autocomplete.cpp) ─────────────────────
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.screens.task_view_store;
import loom.text.parse_references;
import loom.diagnostics.debug;
import loom.tools.agent_runtime;

// ── Agent-menu-only imports (moved out of app_autocomplete.cpp) ─────────
import loom.tools.agent_display;
import loom.ui.features.agents.agent_cards;
import loom.ui.dialogs.system;
import loom.ui.dialogs.triggers;
import loom.ui.permissions.single_prompt;
import loom.ui.permissions.permission_computer_use;
import loom.hooks.cost_hook;

namespace loom::ui {
namespace agent_runtime = loom::tools::agent_runtime;
namespace agent_cards = loom::ui::agents::cards;
// Defined in app_extra_methods.cpp (same module); redeclared for module linkage.
agent_cards::AgentCardData project_agent_definition_card(
    const agent_runtime::AgentDefinition& agent);

std::string FormatAgentsMenuOutput(
    const std::vector<agent_cards::AgentCardData>& agents,
    int selected_position);

namespace {
[[nodiscard]] bool is_built_in_agent(
        const agent_cards::AgentCardData& agent) {
        return agent.source == "built-in";
    }

[[nodiscard]] std::vector<std::size_t> selectable_agent_indices(
        const std::vector<agent_cards::AgentCardData>& agents) {
        std::vector<std::size_t> out;
        out.reserve(agents.size());
        for (std::size_t i = 0; i < agents.size(); ++i) {
            if (!is_built_in_agent(agents[i])) out.push_back(i);
        }
        return out;
    }

[[nodiscard]] std::string agent_model_label(
        const agent_cards::AgentCardData& agent) {
        if (agent.model_override && !agent.model_override->empty()) {
            return *agent.model_override;
        }
        return is_built_in_agent(agent) ? "inherit" : "";
    }

}  // namespace

void AppAdapter::RefreshAgentsMenuOutput() {
        screen_state_->active_local_jsx_content = FormatAgentsMenuOutput(
            screen_state_->task_view_store.agent_cards,
            screen_state_->active_agents_selection_position);
    }

void AppAdapter::OpenAgentsMenu() {
        LoadAgentCardsForMenu();
        screen_state_->mode = repl::ReplMode::AgentsView;
        screen_state_->dialog_store.agents_component.reset();
        this->TriggerStatuslineUpdate();
        PostRenderEvent();
    }

    /// Rebuild live_teammates from native store + pane observer, then open
    /// the TeamsView modal ('teams' footer action).
void AppAdapter::OpenTeamsOverview() {
        ProjectLiveTeammatesToScreenState();
        screen_state_->task_view_store.teams_overview_selected_index = 0;
        loom::ui::dialogs::triggers::PushTeamsView(
            screen_state_->dialog_store.dialog_queue,
            [this] {
                screen_state_->dialog_store.PopModal();
                PostRenderEvent();
            });
        PostRenderEvent();
    }

bool AppAdapter::HandleLocalJsxEvent(const Event& ev) {
        if (!screen_state_->active_local_jsx_command ||
            screen_state_->active_local_jsx_command_name != "agents") {
            return false;
        }

        const auto selectable = selectable_agent_indices(screen_state_->task_view_store.agent_cards);
        const int item_count = 1 + static_cast<int>(selectable.size());
        if (item_count <= 0) return false;

        auto refresh_selection = [&] {
            RefreshAgentsMenuOutput();
            PostRenderEvent();
        };

        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            screen_state_->active_agents_selection_position =
                (screen_state_->active_agents_selection_position + 1) % item_count;
            refresh_selection();
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            screen_state_->active_agents_selection_position =
                (screen_state_->active_agents_selection_position - 1 + item_count) %
                item_count;
            refresh_selection();
            return true;
        }
        if (ev == Event::Return) {
            const int selected = std::clamp(
                screen_state_->active_agents_selection_position,
                0,
                item_count - 1);
            std::string command = "/agents create";
            if (selected > 0) {
                const auto agent_index =
                    selectable[static_cast<std::size_t>(selected - 1)];
                command = "/agents configure " +
                    screen_state_->task_view_store.agent_cards[agent_index].id;
            }
            ClearActiveLocalJsxCommand();
            ResetScrollToBottom(screen_state_->messages_store);
            HandleCommand(command);
            PostRenderEvent();
            return true;
        }
        return false;
    }

namespace repl = loom::ui::repl_screen;
namespace agent_display = loom::tools::agent_display;
namespace dtrig = loom::ui::dialogs::triggers;
namespace dsys = loom::ui::dialogs::system;
namespace cperm = loom::ui::permissions;

// ── FormatAgentsMenuOutput (moved out to remove agent_display import) ────
std::string FormatAgentsMenuOutput(
    const std::vector<agent_cards::AgentCardData>& agents,
    int selected_position) {
    const auto selectable = selectable_agent_indices(agents);
    const int selectable_count = static_cast<int>(selectable.size());
    const int display_count = static_cast<int>(agents.size());
    const int item_count = std::max(1, selectable_count + 1);
    selected_position = std::clamp(selected_position, 0, item_count - 1);

    std::string out;
    out += "Agents\n";
    if (selectable_count == 0) {
        out += "No agents found\n";
    } else {
        out += std::format(
            "{} agent{}\n",
            display_count,
            display_count == 1 ? "" : "s");
    }

    out += "\n";
    out += selected_position == 0 ? "› Create new agent\n"
                                  : "  Create new agent\n";

    if (selectable_count == 0) {
        out += "\n";
        out += "No agents found. Create specialized subagents that Loom can delegate to.\n";
        out += "Each subagent has its own context window, custom system prompt, and specific tools.\n";
        out += "Try creating: Code Reviewer, Code Simplifier, Security Reviewer, Tech Lead, or UX Reviewer.\n";
    } else {
        int position = 1;
        for (const auto& group : agent_display::agent_source_groups()) {
            if (group.source == "built-in") continue;

            bool has_group = false;
            for (const auto& agent : agents) {
                if (agent.source == group.source) {
                    has_group = true;
                    break;
                }
            }
            if (!has_group) continue;

            out += "\n";
            out += group.label;
            out += "\n";
            for (const auto& agent : agents) {
                if (agent.source != group.source) continue;
                out += position == selected_position ? "› " : "  ";
                out += agent.name;
                const auto model = agent_model_label(agent);
                if (!model.empty()) {
                    out += " · ";
                    out += model;
                }
                out += "\n";
                ++position;
            }
        }
    }

    bool has_built_in = false;
    for (const auto& agent : agents) {
        if (is_built_in_agent(agent)) {
            has_built_in = true;
            break;
        }
    }
    if (has_built_in) {
        out += "\n────────────────────────────────────────────────────────────────\n\n";
        out += selectable_count == 0
            ? "Built-in (always available):\n"
            : "Built-in agents (always available)\n";
        for (const auto& agent : agents) {
            if (!is_built_in_agent(agent)) continue;
            out += agent.name;
            const auto model = agent_model_label(agent);
            if (!model.empty()) {
                out += " · ";
                out += model;
            }
            out += "\n";
        }
    }

    out += "\nPress ↑↓ to navigate · Enter to select · Esc to go back";
    return out;
}

// ── LoadAgentCardsForMenu (moved out to remove agent_runtime import) ─────
void AppAdapter::LoadAgentCardsForMenu() {
    std::optional<std::filesystem::path> cwd;
    if (!screen_state_->cwd.empty()) {
        cwd = std::filesystem::path(screen_state_->cwd);
    }

    auto definitions = agent_runtime::get_all_agent_definitions(std::move(cwd));
    std::ranges::sort(definitions, agent_display::CompareAgentsByName{});

    screen_state_->task_view_store.agent_cards.clear();
    screen_state_->task_view_store.agent_cards.reserve(definitions.size());
    for (const auto& definition : definitions) {
        screen_state_->task_view_store.agent_cards.push_back(
            project_agent_definition_card(definition));
    }
    screen_state_->dialog_store.agents_component.reset();
}

// ── SyncState (moved out to remove debug import) ─────────────────────────
void AppAdapter::SyncState() {
    conversation_projection_dirty_.store(true);
    auto messages = static_cast<loom::core::QueryEngine*>(engine_raw())->get_conversation();

    // Bridge / remote-control footer projection (reads replBridge* from
    // AppState).
    if (has_app_store()) {
        const auto b = bridge_state();
        screen_state_->bridge_enabled        = b.enabled;
        screen_state_->bridge_explicit_remote = b.explicit_remote;
        screen_state_->bridge_connected      = b.connected;
        screen_state_->bridge_session_active = b.session_active;
        screen_state_->bridge_reconnecting   = b.reconnecting;
    }

    // Collapse chain for background-bash and similar collapsible rows.
    messages = ApplyMessageCollapsePipeline(std::move(messages));
    loom::utils::debug("app.sync",
        "SyncState: engine has {} messages", messages.size());
    screen_state_->messages_store.messages.clear();
    screen_state_->messages_store.messages.reserve(messages.size());
    // Assign a 24-char prefix per source Message.
    auto make_uuid24 = [](std::uint64_t msg_idx, const std::string& seed) {
        std::uint64_t h = 1469598103934665603ULL;  // FNV offset basis
        for (char c : seed) { h ^= static_cast<std::uint64_t>(c); h *= 1099511628211ULL; }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "msg_%012llx%08llx",
                      (unsigned long long)msg_idx,
                      (unsigned long long)(h & 0xFFFFFFFFULL));
        return std::string(buf, 24);
    };
    std::uint64_t msg_idx = 0;
    for (const auto& msg : messages) {
        if (std::holds_alternative<loom::core::SystemMessage>(msg)) continue;
        auto projected = project_messages(msg);
        std::string seed_preview;
        std::visit([&](const auto& m) {
            if constexpr (requires{ m.content; }) {
                for (const auto& blk : m.content) {
                    if (const auto* tb = std::get_if<loom::core::TextBlock>(&blk)) {
                        seed_preview += tb->text.substr(0, 64);
                        break;
                    }
                }
            }
        }, msg);
        const std::string u24 = make_uuid24(msg_idx, seed_preview);
        for (auto& e : projected) {
            e.id = u24;
            // Enrich committed thinking entries with cached duration
            // and start time (so the chain compressor's live elapsed
            // timer keeps ticking from the original streaming start).
            if (e.is_thinking && e.thinking_duration.count() == 0 &&
                !e.full_content.empty()) {
                auto it = thinking_duration_cache_.find(e.full_content);
                if (it != thinking_duration_cache_.end()) {
                    e.thinking_duration = it->second.duration;
                    e.thinking_start_time = it->second.start_time;
                }
            }
            screen_state_->messages_store.messages.push_back(std::move(e));
        }
        ++msg_idx;
    }
    reconcile_tool_result_statuses(screen_state_->messages_store.messages);
    AppendLocalMessagesToScreenState();
    std::stable_sort(
        screen_state_->messages_store.messages.begin(),
        screen_state_->messages_store.messages.end(),
        [](const repl::MessageDisplayEntry& a,
           const repl::MessageDisplayEntry& b) {
            return a.timestamp < b.timestamp;
        });

    // Debug: log projected message summary
    {
        std::size_t n_user = 0, n_asst = 0, n_sys = 0, n_tool = 0;
        for (const auto& e : screen_state_->messages_store.messages) {
            if (e.role == "user") ++n_user;
            else if (e.role == "assistant") {
                ++n_asst;
                if (e.is_tool_use) ++n_tool;
            }
            else if (e.role == "system") ++n_sys;
        }
        loom::utils::debug("app.sync",
            "SyncState done: {} projected entries "
            "(user={}, asst={}, tool_use={}, sys={})",
            screen_state_->messages_store.messages.size(),
            n_user, n_asst, n_tool, n_sys);
        for (std::size_t i = 0; i < screen_state_->messages_store.messages.size(); ++i) {
            const auto& e = screen_state_->messages_store.messages[i];
            if (e.role == "assistant" && !e.is_tool_use && !e.is_thinking) {
                loom::utils::debug("app.sync",
                    "  msg[{}] assistant text: len={}, streaming={}, preview='{}'",
                    i, e.content_preview.size(), e.is_streaming,
                    e.content_preview.substr(0, 80));
            }
        }
    }

    this->ProjectRuntimeMetadataToScreenState();

    // Live teams projection (native store + pane observer). Runs on the same
    // event-driven cadence as every other SyncState projection — no separate
    // timer; pane changes wake the UI via the observer subscription.
    this->ProjectLiveTeammatesToScreenState();

    // Successful session resume calls SyncState after publishing the new ID.
    // /clear preserves that ID, so running instances remain in this snapshot.
    this->ProjectRuntimeAgentsToScreenState();

    // Notify cost hook subscribers (drives CostThreshold dialog, etc.).
    loom::hooks::update_cost(loom::hooks::CostUpdate{
        .session_cost = screen_state_->chrome_store.status_bar.cost_usd.value_or(0.0),
        .monthly_cost = 0.0,
        .input_tokens = screen_state_->chrome_store.status_bar.input_tokens,
        .output_tokens = screen_state_->chrome_store.status_bar.output_tokens,
    });
}

// ── ConsumePendingResult (moved out to remove debug import) ──────────────
void AppAdapter::ConsumePendingResult() {
    screen_state_->query_running = query_running_.load();
    {
        std::optional<PendingSpinner> spinner;
        bool reset_markdown = false;
        {
            std::lock_guard lk(result_mutex_);
            spinner.swap(pending_spinner_);
            reset_markdown = std::exchange(pending_markdown_reset_, false);
            ProjectGenerationRate();
        }
        if (reset_markdown) streaming_markdown_.reset();
        if (spinner && screen_state_->query_running) {
            repl::SetSpinner(screen_state_->task_view_store, spinner->mode);
            screen_state_->task_view_store.spinner_verb = std::move(spinner->verb);
        }
    }
    if (!screen_state_->query_running && statusline_update_pending_.exchange(false)) {
        TriggerStatuslineUpdate();
    }
    if (auto text = statusline_.TakeResult()) {
        screen_state_->status_line_text = std::move(*text);
    }
    // Drain a completed local '!' bash command first.
    {
        std::optional<PendingBashResult> bash_res;
        if (!screen_state_->query_running) {
            std::lock_guard lk(bash_result_mutex_);
            bash_res.swap(pending_bash_result_);
        }
        if (bash_res && bash_res->session_id == current_session_id_) {
            if (!bash_res->cwd.empty()) {
                std::error_code ec;
                std::filesystem::current_path(bash_res->cwd, ec);
                if (!ec) {
                    screen_state_->cwd = bash_res->cwd;
                    static_cast<loom::core::QueryEngine*>(engine_raw())->set_working_directory(bash_res->cwd);
                }
            }
            AppendLocalCommandMessage(
                bash_res->output.empty()
                    ? std::string("(no output)")
                    : std::move(bash_res->output),
                bash_res->is_error);
        }
    }

    if (query_running_.load()) return;
    if (!repl::IsToolAnimating(screen_state_->task_view_store)) return;

    loom::utils::debug("app.consume",
        "ConsumePendingResult firing — spinner_mode={}, calling SyncState",
        static_cast<int>(screen_state_->task_view_store.spinner_mode));

    std::lock_guard lk(result_mutex_);

    auto pending_error = std::move(pending_error_);
    pending_error_.reset();

    this->SyncState();
    if (pending_error && !pending_error->message.empty()) {
        repl::MessageDisplayEntry error_entry;
        error_entry.role = "system";
        error_entry.content_preview = pending_error->message;
        error_entry.is_error = true;
        error_entry.timestamp = std::chrono::system_clock::now();
        error_entry.id = "err_00000000000000000000";
        const auto& type = pending_error->type;
        error_entry.can_continue = type == "tool_round_limit";
        error_entry.is_error = !error_entry.can_continue;
        error_entry.is_model_error = type != "budget_exceeded" &&
            type != "invalid_retry" && type != "hook_stop" && type != "tool_round_limit";
        error_entry.can_retry = type == "api_error" || type == "overloaded_error" ||
            type == "rate_limit_error" || type == "network_error" ||
            type == "transport_error";
        screen_state_->messages_store.messages.push_back(std::move(error_entry));
    }
    repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::Hidden);
    screen_state_->task_view_store.spinner_verb = std::nullopt;
    screen_state_->task_view_store.spinner_tip = std::nullopt;
    streaming_text_.clear();
    streaming_text_index_.reset();
    streaming_finalizing_ = false;
    streaming_message_committed_ = false;
    streaming_markdown_.reset();
    streaming_tools_.clear();

    // The engine persists every message incrementally to messages.jsonl
    // (with full thinking/tool_use/tool_result blocks) via
    // QueryEngine::append_message().  No separate save step is needed — and
    // the old text-only save that downgraded rich messages to plain text was
    // removed, since it made resume fall back to a degraded transcript with
    // no chain compression or tool calls.

    this->TriggerStatuslineUpdate();
}

// Submissions wait for clipboard completion through the worker's Custom event.
bool AppAdapter::HasInFlightPasteReferences(const std::string& text) const {
    const auto refs = loom::utils::parse_references(text);
    return std::ranges::any_of(refs, [this](const auto& r) {
        return paste_.is_in_flight(r.id);
    });
}

void AppAdapter::DrainCompletedPastesAndSubmit() {
    if (!pending_paste_submission_) {
        ProcessCompletedPastes();
        return;
    }
    // Complete text/failure replacements in the submitted snapshot while
    // preserving the draft the user may already be editing.
    auto draft_cursor = screen_state_->input_cursor;
    screen_state_->input_text.swap(pending_paste_submission_->text);
    screen_state_->input_cursor = std::string::npos;
    std::unordered_set<int> needed;
    for (const auto& ref : loom::utils::parse_references(screen_state_->input_text))
        needed.insert(ref.id);
    paste_.set_pending_drain_ids(std::move(needed));
    ProcessCompletedPastes();
    paste_.set_pending_drain_ids(std::nullopt);
    screen_state_->input_text.swap(pending_paste_submission_->text);
    screen_state_->input_cursor = draft_cursor;
    if (HasInFlightPasteReferences(pending_paste_submission_->text) &&
        std::chrono::steady_clock::now() < pending_paste_submission_->deadline) return;
    auto submission = std::move(*pending_paste_submission_);
    pending_paste_submission_.reset();
    paste_submission_deadline_ms_.store(0);
    HandleSubmit(submission.text, submission.mode, /*wait_for_pastes=*/false);
    ProcessCompletedPastes();
}

// ── get_permission_callback (moved out to remove dialogs.system/triggers) ─
std::function<bool(std::string_view, std::string_view)> AppAdapter::get_permission_callback() {
    return [this, dispatch = ui_dispatcher(), permission = permission_](
        std::string_view tool_name, std::string_view tool_args) -> bool {
        // Fast path: always-allowed tools skip the dialog entirely.
        if (permission->is_always_allowed(tool_name))
            return true;

        const auto token = permission->begin_permission_request();
        if (token == 0) return false;
        if (permission->is_always_allowed(tool_name)) {
            permission->finish_permission_request(token);
            return true;
        }
        const auto dialog_id = std::format("tool-permission-{}", token);
        dispatch([this, permission, token, dialog_id,
                  tool_name = std::string(tool_name), tool_args = std::string(tool_args)] {
            if (!permission->permission_request_current(token)) return;
            dsys::ToolPermissionPayload payload;
            payload.id = dialog_id;
            payload.tool_name = tool_name;
            payload.description = tool_args;
            payload.can_always_allow = true;
            payload.initial_sandbox_toggle = false;
            payload.on_response = [this, permission, token, dialog_id, tool_name](
                dsys::ToolPermissionPayload::Decision decision, bool /*sandbox*/) {
                // Removing this exact overlay can destroy the currently invoked
                // callback. Keep everything needed afterward in local storage.
                auto* app = this;
                auto coordinator = permission;
                const auto request = token;
                const auto id = dialog_id;
                const auto tool = tool_name;
                using Decision = dsys::ToolPermissionPayload::Decision;
                const bool allowed = decision == Decision::AllowOnce ||
                                     decision == Decision::AlwaysAllow;
                if (coordinator->resolve_permission(request, allowed,
                    decision == Decision::AlwaysAllow ? std::string_view{tool}
                                                      : std::string_view{})) {
                    // Reflect the user's decision in this UI event. The worker
                    // still owns the slot until its dispatched cleanup finishes.
                    app->screen_state_->dialog_store.RemoveDialog(id);
                }
            };
            payload.on_abort = [this, permission, token, dialog_id] {
                auto* app = this;
                auto coordinator = permission;
                const auto request = token;
                const auto id = dialog_id;
                if (coordinator->resolve_permission(request, false)) {
                    app->screen_state_->dialog_store.RemoveDialog(id);
                }
            };
            if (auto cu_options = cperm::options_from_tool_input(tool_args)) {
                namespace sp = loom::ui::permissions::single_prompt;
                sp::DetailComputerUse detail;
                detail.action_label = std::string(cperm::action_description(cu_options->action));
                detail.target_app = cu_options->target_app;
                detail.coordinates = cu_options->coordinates;
                detail.text_to_type = cu_options->text_to_type;
                detail.first_use_in_session = permission->take_computer_use_first_use();
                payload.description = std::format("Loom wants to control your screen: {}",
                    cperm::action_description(cu_options->action));
                payload.action_kind = sp::ActionKind::Execute;
                payload.detail = sp::ToolDetail{std::move(detail)};
            }
            screen_state_->dialog_store.PushDialog(std::move(payload));
        });
        const bool allowed = permission->wait_permission_response(token);
        dispatch([this, permission, token, dialog_id] {
            if (!permission->permission_request_current(token)) return;
            screen_state_->dialog_store.RemoveDialog(dialog_id);
            permission->finish_permission_request(token);
        });
        return allowed;
    };
}

// ── trigger_orphan_cleanup_for_testing (moved out for parse_references) ──
void AppAdapter::trigger_orphan_cleanup_for_testing() {
    if (paste_.pasted_contents().empty()) return;
    const auto refs = loom::utils::parse_references(screen_state_->input_text);
    std::unordered_set<int> referenced_ids;
    for (const auto& r : refs) referenced_ids.insert(r.id);
    for (auto it = paste_.pasted_contents().begin(); it != paste_.pasted_contents().end(); ) {
        if (!referenced_ids.contains(it->first)) {
            it = paste_.pasted_contents().erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace loom::ui
