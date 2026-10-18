// RFC 0023: session-scoped composition and explicit host controls.
module;
#include <ftxui/component/component.hpp>
module loom.ui.app.app;
import std;
import loom.query.query_engine;
import loom.tools.agent_runtime;
import loom.ui.dialogs.system;
import loom.ui.features.agents.instance_browser;
import loom.ui.features.agents.runtime_agents;

namespace loom::ui {
namespace {
namespace native = loom::tools::agent_runtime;
namespace browser = loom::ui::agents::instances;
namespace ds = loom::ui::dialogs::system;

struct InstanceSnapshotCache {
    std::uint64_t list_generation = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t detail_generation = std::numeric_limits<std::uint64_t>::max();
    std::vector<browser::Entry> entries;
    std::optional<browser::Detail> detail;
    std::string detail_id;
    bool parent_busy = false;
};
std::string detail_tail(std::string_view text) {
    constexpr std::size_t limit = 16384;
    if (text.size() <= limit) return std::string(text);
    text.remove_prefix(text.size() - limit);
    while (!text.empty() && (static_cast<unsigned char>(text.front()) & 0xc0) == 0x80)
        text.remove_prefix(1);
    return "[truncated; showing tail]\n" + std::string(text);
}

browser::Entry instance_entry(const native::NativeAgentRecord& record) {
    browser::Entry entry;
    entry.id = record.agent_id;
    entry.name = record.name.value_or("");
    entry.type = record.agent_type;
    entry.description = record.description.value_or("");
    entry.team_name = record.team_name.value_or("");
    entry.team = native::is_team_agent_record(record);
    switch (record.status) {
        case native::NativeAgentStatus::Queued: entry.status = agents::runtime::Status::Queued; break;
        case native::NativeAgentStatus::Running: entry.status = agents::runtime::Status::Running; break;
        case native::NativeAgentStatus::Completed: entry.status = agents::runtime::Status::Completed; break;
        case native::NativeAgentStatus::Failed: entry.status = agents::runtime::Status::Failed; break;
        case native::NativeAgentStatus::Cancelled: entry.status = agents::runtime::Status::Cancelled; break;
    }
    entry.active = agents::runtime::is_active(entry.status);
    entry.cancelling = record.cancel_requested && entry.active;
    return entry;
}

bool pending_instance_permission(repl::ReplScreenState& state) {
    const auto actionable = [](const auto& ref) {
        if (!ref) return false;
        return std::visit([](const auto& request) {
            using T = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<T, ds::ToolPermissionPayload> ||
                          std::is_same_v<T, ds::SandboxPermissionPayload> ||
                          std::is_same_v<T, ds::WorkerSandboxPermissionPayload>)
                return static_cast<bool>(request.on_response);
            return false;
        }, ref->get());
    };
    return actionable(state.dialog_store.dialog_queue.peek_overlay_mut()) ||
        actionable(state.dialog_store.dialog_queue.peek_bottom_mut(false, true));
}
} // namespace

void AppAdapter::OpenAgentInstances(std::string_view argument) {
    if (pending_instance_permission(*screen_state_)) {
        screen_state_->footer_dynamic_text = "Respond to the permission request before opening instances";
        PostRenderEvent();
        return;
    }
    if (current_session_id_.empty()) return;
    if (agent_instances_session_ != current_session_id_) agent_instances_state_.reset();
    agent_instances_session_ = current_session_id_;
    if (!agent_instances_state_) agent_instances_state_ = std::make_shared<browser::BrowserState>();
    auto state = std::static_pointer_cast<browser::BrowserState>(agent_instances_state_);
    if (argument.empty() || argument == "history" || argument == "team") {
        const bool team = argument == "team";
        if (state->team != team) state->selected_id.clear();
        state->team = team;
        state->history = argument == "history";
    } else {
        auto record = native::native_agent_store().get(argument);
        if (!record || !record->parent_session_id || *record->parent_session_id != current_session_id_) {
            AppendLocalCommandMessage("No instance with that ID in this session", true, true);
            return;
        }
        state->selected_id = record->agent_id;
        state->team = native::is_team_agent_record(*record);
        state->history = !agents::runtime::is_active(instance_entry(*record).status);
        OpenAgentConversation(current_session_id_, record->agent_id);
        return;
    }
    const auto session = current_session_id_;
    auto cache = std::make_shared<InstanceSnapshotCache>();
    browser::Options options;
    options.open_detail = !argument.empty() && argument != "history" && argument != "team";
    options.state = state;
    options.list = [this, session, cache] {
        if (session != current_session_id_) return std::vector<browser::Entry>{};
        if (cache->list_generation == runtime_agent_generation_) return cache->entries;
        std::vector<browser::Entry> entries;
        auto records = native::native_agent_store().list();
        std::ranges::sort(records, [](const auto& a, const auto& b) {
            return a.created_at != b.created_at ? a.created_at < b.created_at : a.agent_id < b.agent_id;
        });
        for (const auto& record : records)
            if (record.parent_session_id && *record.parent_session_id == session)
                entries.push_back(instance_entry(record));
        cache->entries = std::move(entries);
        cache->list_generation = runtime_agent_generation_;
        return cache->entries;
    };
    options.detail = [this, session, cache](std::string_view id) -> std::optional<browser::Detail> {
        if (session != current_session_id_) return std::nullopt;
        const bool busy = query_running_.load();
        if (cache->detail_id == id && cache->detail_generation == runtime_agent_generation_ &&
            cache->parent_busy == busy) return cache->detail;
        auto record = native::native_agent_store().get(id);
        if (!record || !record->parent_session_id || *record->parent_session_id != session) return std::nullopt;
        browser::Detail detail;
        detail.entry = instance_entry(*record);
        detail.error = detail_tail(record->error.value_or(""));
        detail.output = detail_tail(record->output.value_or(""));
        const auto begin = record->transcript.size() > 80 ? record->transcript.size() - 80 : 0;
        if (begin != 0) detail.transcript = "[Earlier transcript omitted]\n";
        for (std::size_t i = begin; i < record->transcript.size(); ++i) {
            const auto& line = record->transcript[i];
            detail.transcript += line.size() > 16384 ? line.substr(line.size() - 16384) : line;
            detail.transcript += '\n';
            if (detail.transcript.size() > 32768)
                detail.transcript = "[Earlier transcript omitted]\n" + detail.transcript.substr(detail.transcript.size() - 16384);
        }
        auto* engine = static_cast<loom::core::QueryEngine*>(engine_raw());
        detail.continue_reason = query_running_.load() ? "Wait for the parent operation to finish" : engine->agent_continue_unavailable_reason(id);
        detail.stop_reason = engine->agent_stop_unavailable_reason(id);
        detail.can_continue = detail.continue_reason.empty();
        detail.can_stop = detail.stop_reason.empty();
        detail.transcript = detail_tail(detail.transcript);
        cache->detail_id = std::string(id);
        cache->detail_generation = runtime_agent_generation_;
        cache->parent_busy = busy;
        cache->detail = std::move(detail);
        return cache->detail;
    };
    options.on_continue = [this, session](std::string id, std::string prompt) {
        ContinueAgentInstance(session, std::move(id), std::move(prompt));
    };
    options.on_stop = [this, session](std::string id) { StopAgentInstance(session, id); };
    options.on_open = [this, session](std::string id) { OpenAgentConversation(session, std::move(id)); };
    options.on_close = [this] {
        screen_state_->dialog_store.dialog_queue.remove(agent_instances_dialog_id_);
        agent_instances_dialog_id_.clear();
    };
    ds::AgentInstancesPayload payload;
    payload.id = "agent-instances-" + session;
    payload.component = std::make_shared<Component>(browser::CreateBrowser(std::move(options)));
    payload.on_resize = [state](int columns, int rows) {
        state->columns = columns;
        state->rows = rows;
    };
    screen_state_->dialog_store.dialog_queue.remove(agent_instances_dialog_id_);
    agent_instances_dialog_id_ = screen_state_->dialog_store.dialog_queue.push(std::move(payload));
    PostRenderEvent();
}

void AppAdapter::ContinueAgentInstance(std::string session, std::string id, std::string prompt) {
    if (session != current_session_id_ || query_running_.load()) {
        AppendLocalCommandMessage("Cannot continue while another parent operation is active", true, true);
        return;
    }
    if (query_thread_.joinable()) query_thread_.join();
    ConsumePendingResult();
    if (agent_operation_thread_.joinable()) agent_operation_thread_.join();
    if (query_running_.exchange(true)) return;
    screen_state_->dialog_store.dialog_queue.remove(agent_instances_dialog_id_);
    agent_instances_dialog_id_.clear();
    screen_state_->query_running = true;
    const auto generation = BeginProgramStatusOperation();
    try {
    agent_operation_thread_ = std::jthread([this, session = std::move(session), id = std::move(id),
        prompt = std::move(prompt), generation, dispatch = ui_dispatcher()](std::stop_token) {
        bool failed = true;
        std::string notice;
        try {
            auto result = static_cast<loom::core::QueryEngine*>(engine_raw())->continue_agent_instance(id, prompt);
            failed = !result;
            notice = result ? "Continuation started. Open /subagents to follow progress." : result.error().format();
        } catch (const std::exception& error) { notice = error.what(); }
        catch (...) { notice = "Continuation launch failed"; }
        dispatch([this, session, generation, failed, notice = std::move(notice)] {
            query_running_.store(false);
            screen_state_->query_running = false;
            CompleteProgramStatusOperation(generation, failed, false);
            if (session == current_session_id_) AppendLocalCommandMessage(notice, failed, true);
            runtime_agents_dirty_->store(true);
        });
    });
    } catch (const std::exception& error) {
        query_running_.store(false);
        screen_state_->query_running = false;
        CompleteProgramStatusOperation(generation, true, false);
        AppendLocalCommandMessage(error.what(), true, true);
    }
    PostRenderEvent();
}

void AppAdapter::StopAgentInstance(std::string_view session, std::string_view id) {
    if (session != current_session_id_) return;
    auto result = static_cast<loom::core::QueryEngine*>(engine_raw())->stop_agent_instance(id);
    if (!result) {
        screen_state_->footer_dynamic_text = result.error().format();
        screen_state_->footer_dynamic_color = "error";
    } else screen_state_->footer_dynamic_text = "Cancellation requested";
    const auto child = agent_conversation_views_.find(std::string(id));
    if (child != agent_conversation_views_.end())
        child->second.state->footer_dynamic_text = screen_state_->footer_dynamic_text;
    runtime_agents_dirty_->store(true);
    PostRenderEvent();
}
} // namespace loom::ui
