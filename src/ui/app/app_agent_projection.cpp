// RFC 0021: session-owned ordinary instances and lifetime-safe UI notifications.
module loom.ui.app.app;

import std;
import loom.tools.agent_runtime;
import loom.ui.features.agents.runtime_agents;

namespace loom::ui {

std::function<void()> AppAdapter::MakeRuntimeAgentChangeCallback() {
    return [dirty = runtime_agents_dirty_, dispatch = ui_dispatcher()] {
        if (!dirty->exchange(true, std::memory_order_acq_rel)) dispatch({});
    };
}

void AppAdapter::SubscribeRuntimeAgentChanges() {
    runtime_agents_subscription_ = loom::tools::agent_runtime::native_agent_store()
        .subscribe_changed(MakeRuntimeAgentChangeCallback());
    try {
        ProjectRuntimeAgentsToScreenState();
    } catch (...) {
        CloseUiActions();
        UnsubscribeRuntimeAgentChanges();
        throw;
    }
}

void AppAdapter::UnsubscribeRuntimeAgentChanges() {
    if (runtime_agents_subscription_ == 0) return;
    loom::tools::agent_runtime::native_agent_store().unsubscribe_changed(runtime_agents_subscription_);
    runtime_agents_subscription_ = 0;
}

void AppAdapter::ProjectRuntimeAgentsToScreenState() {
    if (agent_instances_session_ != current_session_id_) {
        screen_state_->dialog_store.dialog_queue.remove(agent_instances_dialog_id_);
        agent_instances_dialog_id_.clear();
        agent_instances_state_.reset();
        active_agent_conversation_.clear();
        screen_state_->prioritize_actionable_dialogs = false;
        agent_conversation_views_.clear();
        agent_instances_session_ = current_session_id_;
    }
    const bool changed = runtime_agents_dirty_->exchange(false, std::memory_order_acq_rel);
    if (!changed && projected_runtime_agent_session_ == current_session_id_) return;
    ++runtime_agent_generation_;
    namespace native = loom::tools::agent_runtime;
    namespace display = loom::ui::agents::runtime;
    auto records = native::native_agent_store().list();
    std::erase_if(records, [&](const native::NativeAgentRecord& record) {
        return !record.parent_session_id || *record.parent_session_id != current_session_id_ ||
            native::is_team_agent_record(record);
    });
    auto active = [](native::NativeAgentStatus status) {
        return status == native::NativeAgentStatus::Queued || status == native::NativeAgentStatus::Running;
    };
    std::ranges::sort(records, [&](const auto& left, const auto& right) {
        if (active(left.status) != active(right.status)) return active(left.status);
        if (left.created_at != right.created_at) return left.created_at < right.created_at;
        return left.agent_id < right.agent_id;
    });
    std::vector<display::Instance> projected;
    projected.reserve(records.size());
    for (const auto& record : records) {
        display::Status status = display::Status::Queued;
        switch (record.status) {
            case native::NativeAgentStatus::Queued: status = display::Status::Queued; break;
            case native::NativeAgentStatus::Running: status = display::Status::Running; break;
            case native::NativeAgentStatus::Completed: status = display::Status::Completed; break;
            case native::NativeAgentStatus::Failed: status = display::Status::Failed; break;
            case native::NativeAgentStatus::Cancelled: status = display::Status::Cancelled; break;
        }
        projected.push_back(display::Instance{
            record.agent_id, record.name.value_or(""), record.description.value_or(""),
            status, record.background});
    }
    if (screen_state_->task_view_store.runtime_agents != projected)
        screen_state_->task_view_store.runtime_agents = std::move(projected);
    projected_runtime_agent_session_ = current_session_id_;
    // Native team changes use the same wake even while the parent is busy.
    // The ordinary projection above deliberately excludes these records.
    if (changed) ProjectLiveTeammatesToScreenState();
}

} // namespace loom::ui
