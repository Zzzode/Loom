// RFC 0024: retained native conversation views. Root engine ownership never moves.
module;
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
module loom.ui.app.app;
import std;
import loom.query.query_engine;
import loom.query.assembly;
import loom.serdes.json;
import loom.tools.agent_runtime;
import loom.ui.dialogs.system;
import loom.ui.screens.repl_screen;

namespace loom::ui {
namespace native = loom::tools::agent_runtime;
namespace ds = loom::ui::dialogs::system;

bool AppAdapter::RootDialogTakesPriority() {
    screen_state_->prioritize_actionable_dialogs = false;
    auto& queue = screen_state_->dialog_store.dialog_queue;
    const auto actionable = [this](const auto& ref) {
        if (!ref) return false;
        const auto& payload = ref->get();
        if (!screen_state_->dialog_store.dialog_renderers.has_renderer_and_handler(
                ds::type_of(payload)))
            return false;
        return std::visit(
            [](const auto& request) {
                using T = std::decay_t<decltype(request)>;
                if constexpr (std::is_same_v<T, ds::PromptDialogPayload> ||
                              std::is_same_v<T, ds::ElicitationPayload> ||
                              std::is_same_v<T, ds::ToolPermissionPayload> ||
                              std::is_same_v<T, ds::SandboxPermissionPayload> ||
                              std::is_same_v<T, ds::WorkerSandboxPermissionPayload>)
                    return static_cast<bool>(request.on_response);
                return false;
            },
            payload);
    };
    const bool prompt =
        actionable(queue.peek_overlay_mut()) || actionable(queue.peek_bottom_mut(false, true));
    screen_state_->prioritize_actionable_dialogs = !active_agent_conversation_.empty() && prompt;
    return queue.has_standalone() || queue.has_modal() || queue.has_overlay() || prompt;
}

Component AppAdapter::VisibleConversationComponent() {
    const bool dialog_priority = RootDialogTakesPriority();
    if (active_agent_conversation_.empty() || dialog_priority) return repl_component_;
    const auto found = agent_conversation_views_.find(active_agent_conversation_);
    return found == agent_conversation_views_.end() ? repl_component_ : found->second.component;
}

void AppAdapter::OpenAgentConversation(std::string session, std::string id) {
    if (session.empty() || session != current_session_id_) return;
    auto record = native::native_agent_store().get(id);
    if (!record || !record->parent_session_id || *record->parent_session_id != session) return;
    screen_state_->dialog_store.dialog_queue.remove(agent_instances_dialog_id_);
    agent_instances_dialog_id_.clear();
    auto [it, inserted] = agent_conversation_views_.try_emplace(id);
    auto& view = it->second;
    if (inserted) {
        view.root_session = session;
        view.id = id;
        view.state = std::make_shared<repl::ReplScreenState>();
        // Native workers accept input independently of the root query lifecycle.
        // Individual cards retain their actual tool status without a root chain.
        view.state->disable_chain_compression = true;
        view.state->cwd = record->cwd.value_or(screen_state_->cwd);
        view.state->status_line_text = "Agent session · usage unavailable";
        view.state->prompt_store.viewing_agent_name = record->name.value_or(id);
        repl::ReplScreenCallbacks callbacks;
        callbacks.on_submit_checked = [this, session, id](const std::string& text,
                                                          repl::InputMode mode) {
            return SubmitAgentConversation(session, id, text, mode);
        };
        callbacks.on_interrupt = [this, session, id] { StopAgentInstance(session, id); };
        callbacks.on_exit = [this] {
            if (on_exit_) on_exit_();
        };
        view.component = repl::ReplScreen(view.state, std::move(callbacks));
    }
    active_agent_conversation_ = id;
    RefreshAgentConversation();
    PostRenderEvent();
}

void AppAdapter::RefreshAgentConversation() {
    if (active_agent_conversation_.empty()) return;
    auto found = agent_conversation_views_.find(active_agent_conversation_);
    if (found == agent_conversation_views_.end()) return;
    auto& view = found->second;
    if (view.root_session != current_session_id_) {
        active_agent_conversation_.clear();
        return;
    }
    if (view.generation == runtime_agent_generation_) return;
    auto record = native::native_agent_store().get(view.id);
    if (!record || !record->parent_session_id || *record->parent_session_id != view.root_session) {
        view.state->footer_dynamic_text =
            "Agent no longer belongs to this session. /back returns to Main.";
        return;
    }
    const bool running = record->status == native::NativeAgentStatus::Running ||
                         record->status == native::NativeAgentStatus::Queued;
    const auto status = record->cancel_requested && running
                            ? "stopping; approval may still need a decision"
                            : native::native_agent_status_name(record->status);
    view.title = std::format("Main › {} · {} · {}",
                             native::is_team_agent_record(*record) ? "Team Agent" : "Subagent",
                             status, record->name.value_or(view.id));
    view.state->status_bar_enabled = false;
    view.state->conversation_breadcrumb = view.title;
    view.state->conversation_return_hint = "Alt+← Main · Esc list · Ctrl-C Stop";
    // This flag controls editor affordances, not a second engine or worker.
    view.state->query_running = false;
    view.state->status_line_text = "Agent session · usage unavailable";
    std::vector<repl::MessageDisplayEntry> rows;
    std::size_t index = 0;
    std::size_t unavailable_entries = 0;
    for (const auto& entry : record->sidechain_entries) {
        const auto parsed = loom::utils::json::parse(entry);
        if (!parsed) {
            ++index;
            ++unavailable_entries;
            continue;
        }
        auto root = parsed->root();
        auto source = root.get("message");
        if (!source.is_obj()) source = root;
        auto message = loom::query::parse_session_message_value(source, index);
        if (!message) {
            ++index;
            ++unavailable_entries;
            continue;
        }
        const auto uuid = root.get_string("uuid");
        const auto identity = uuid.empty() ? std::format("{}-{}", view.id, index) : uuid;
        std::uint64_t hash = 1469598103934665603ULL;
        for (const auto ch : identity) {
            hash ^= static_cast<unsigned char>(ch);
            hash *= 1099511628211ULL;
        }
        const auto stable_id = std::format("agt_{:012x}{:08x}", index, hash & 0xffffffffULL);
        std::visit(
            [&](auto& value) {
                value.id.value = stable_id;
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>, UserMessage>)
                    value.origin = HumanInput{};
                value.timestamp = record->created_at + std::chrono::milliseconds(index);
            },
            *message);
        auto projected = project_messages(*message);
        for (auto& row : projected) {
            row.id = stable_id;
            rows.push_back(std::move(row));
        }
        ++index;
    }
    // A retained call has no successful result until its own tool_use_id matches.
    for (auto& row : rows)
        if (row.is_tool_use)
            row.tool_status =
                record->status == native::NativeAgentStatus::Running ? "running" : "pending";
    reconcile_tool_result_statuses(rows);
    if (!record->live_text.empty()) {
        repl::MessageDisplayEntry live;
        live.id = view.id + "-live";
        live.role = "assistant";
        live.content_preview = record->live_text;
        live.full_content = record->live_text;
        live.is_streaming = true;
        live.timestamp = record->updated_at;
        rows.push_back(std::move(live));
    }
    for (std::size_t i = 0; i < record->pending_messages.size(); ++i) {
        repl::MessageDisplayEntry queued;
        queued.id = std::format("{}-queued-{}", view.id, i);
        queued.role = "user";
        queued.content_preview =
            std::format("[Queued{}] {}", running ? "" : "; explicit continuation required",
                        record->pending_messages[i]);
        queued.timestamp = record->updated_at;
        rows.push_back(std::move(queued));
    }
    if (unavailable_entries != 0) {
        repl::MessageDisplayEntry warning;
        warning.id = view.id + "-unavailable-history";
        warning.role = "assistant";
        warning.content_preview = std::format("{} retained history entries could not be decoded. "
                                              "Details remain available from the instance list.",
                                              unavailable_entries);
        warning.is_error = true;
        rows.push_back(std::move(warning));
    }
    if (rows.empty()) {
        repl::MessageDisplayEntry unavailable;
        unavailable.id = view.id + "-empty";
        unavailable.role = "assistant";
        unavailable.content_preview = "No structured conversation has been retained for this Agent "
                                      "yet. Press Esc, then i for Details.";
        rows.push_back(std::move(unavailable));
    }
    view.state->messages_store.messages = std::move(rows);
    view.generation = runtime_agent_generation_;
}

bool AppAdapter::SubmitAgentConversation(std::string session, std::string id,
                                         const std::string& text, repl::InputMode mode) {
    auto found = agent_conversation_views_.find(id);
    if (session != current_session_id_ || found == agent_conversation_views_.end()) return false;
    auto state = found->second.state;
    const auto refuse = [&](std::string reason) {
        state->footer_dynamic_text = std::move(reason);
        PostRenderEvent();
        return false;
    };
    if (text == "/back") {
        active_agent_conversation_.clear();
        PostRenderEvent();
        return true;
    }
    if (text == "/exit") {
        if (on_exit_) on_exit_();
        return true;
    }
    if (text == "/stop") {
        StopAgentInstance(session, id);
        return true;
    }
    if (text == "/subagents" || text == "/subagents history" || text == "/subagents team") {
        OpenAgentInstances(text.size() > 11 ? std::string_view(text).substr(11)
                                            : std::string_view{});
        return true;
    }
    if (mode != repl::InputMode::Normal || text.starts_with('!') || text.starts_with('/'))
        return refuse(
            "This Agent session supports text only. /back returns to Main for other commands.");
    if (text.find("[Image #") != std::string::npos ||
        text.find("[Pasted text #") != std::string::npos)
        return refuse("Attachments are unavailable in this Agent session; the draft was retained.");
    auto record = native::native_agent_store().get(id);
    if (!record) return refuse("Agent unavailable; the draft was retained.");
    auto* engine = static_cast<loom::core::QueryEngine*>(engine_raw());
    const bool active = record->status == native::NativeAgentStatus::Running ||
                        record->status == native::NativeAgentStatus::Queued;
    if (active) {
        auto result = engine->send_agent_instance_message(id, text);
        if (!result) return refuse(result.error().format());
        state->footer_dynamic_text =
            "Queued for this Agent; processing starts at its next available turn.";
        runtime_agents_dirty_->store(true);
        PostRenderEvent();
        return true;
    }
    if (query_running_.load()) return refuse("Wait for parent operation to finish.");
    auto reason = engine->agent_message_unavailable_reason(id);
    if (!reason.empty()) return refuse(std::move(reason));
    if (query_thread_.joinable()) query_thread_.join();
    ConsumePendingResult();
    if (agent_operation_thread_.joinable()) agent_operation_thread_.join();
    if (query_running_.exchange(true))
        return refuse("Another parent operation is active; the draft was retained.");
    screen_state_->query_running = true;
    const auto generation = BeginProgramStatusOperation();
    try {
        agent_operation_thread_ = std::jthread([this, session, id, text, state, generation,
                                                dispatch = ui_dispatcher()](std::stop_token) {
            bool failed = true;
            std::string notice;
            try {
                auto result = static_cast<loom::core::QueryEngine*>(engine_raw())
                                  ->send_agent_instance_message(id, text);
                failed = !result;
                notice = result ? "Continuation started for this Agent." : result.error().format();
            } catch (const std::exception& error) {
                notice = error.what();
            } catch (...) {
                notice = "Agent message launch failed";
            }
            dispatch([this, session, text, state, generation, failed, notice = std::move(notice)] {
                query_running_.store(false);
                screen_state_->query_running = false;
                CompleteProgramStatusOperation(generation, failed, false);
                if (session == current_session_id_) {
                    state->footer_dynamic_text = notice;
                    if (failed && state->input_text.empty())
                        repl::set_prompt_input_text(state, text, text.size());
                }
                runtime_agents_dirty_->store(true);
            });
        });
    } catch (const std::exception& error) {
        query_running_.store(false);
        screen_state_->query_running = false;
        CompleteProgramStatusOperation(generation, true, false);
        return refuse(error.what());
    }
    return true;
}

bool AppAdapter::HandleAgentConversationEvent(const Event& event) {
    if (active_agent_conversation_.empty() || RootDialogTakesPriority()) return false;
    if (event == Event::Special("\x1b[1;3D") || event == Event::Special("\x1b\x1b[D")) {
        active_agent_conversation_.clear();
        PostRenderEvent();
        return true;
    }
    if (event == Event::Escape) {
        OpenAgentInstances({});
        return true;
    }
    if (event == Event::Character('\x03')) {
        StopAgentInstance(current_session_id_, active_agent_conversation_);
        return true;
    }
    if (event == Event::Character('\x16')) {
        agent_conversation_views_.at(active_agent_conversation_).state->footer_dynamic_text =
            "Attachment paste is unavailable here. Paste text directly or use /back.";
        return true;
    }
    return VisibleConversationComponent()->OnEvent(event);
}
}  // namespace loom::ui
