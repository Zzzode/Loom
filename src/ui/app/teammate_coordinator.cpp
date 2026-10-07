// teammate_coordinator.cpp — impl unit for loom.ui.app.teammate_coordinator

module;

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

module loom.ui.app.teammate_coordinator;

import std;

namespace loom::ui {

struct TeammateCoordinator::Impl {
    // Leader-side teammate permission requests (drained into the dialog).
    std::mutex teammate_permission_mutex_;
    std::deque<PendingTeammatePermission> teammate_pending_permissions_;

    // Pane-teammate inbox worker state.
    std::jthread teammate_inbox_thread_;
    std::mutex teammate_pending_mutex_;
    std::deque<std::string> teammate_pending_prompts_;
    std::unordered_set<std::string> teammate_seen_message_ids_;
    std::string teammate_self_agent_id_;
    std::string teammate_self_agent_name_;
    std::string teammate_self_team_;
};

TeammateCoordinator::TeammateCoordinator() : impl_(std::make_unique<Impl>()) {}
TeammateCoordinator::~TeammateCoordinator() = default;

// ── Self-contained methods ──────────────────────────────────────────────

bool TeammateCoordinator::running_as_pane_teammate() const {
    return !impl_->teammate_self_agent_name_.empty() &&
           !impl_->teammate_self_team_.empty();
}

void TeammateCoordinator::configure_for_testing(
    std::string agent_name, std::string team) {
    impl_->teammate_self_agent_name_ = std::move(agent_name);
    impl_->teammate_self_team_ = std::move(team);
}

std::size_t TeammateCoordinator::pending_prompt_count() const {
    std::lock_guard lock(impl_->teammate_pending_mutex_);
    return impl_->teammate_pending_prompts_.size();
}

std::string TeammateCoordinator::pop_prompt_for_testing() {
    std::lock_guard lock(impl_->teammate_pending_mutex_);
    if (impl_->teammate_pending_prompts_.empty()) return {};
    std::string out = std::move(impl_->teammate_pending_prompts_.front());
    impl_->teammate_pending_prompts_.pop_front();
    return out;
}

std::size_t TeammateCoordinator::pending_permission_count() const {
    std::lock_guard lock(impl_->teammate_permission_mutex_);
    return impl_->teammate_pending_permissions_.size();
}

// ── State accessors ─────────────────────────────────────────────────────

std::mutex& TeammateCoordinator::pending_mutex() noexcept {
    return impl_->teammate_pending_mutex_;
}

std::deque<std::string>& TeammateCoordinator::pending_prompts() noexcept {
    return impl_->teammate_pending_prompts_;
}

std::unordered_set<std::string>& TeammateCoordinator::seen_message_ids() noexcept {
    return impl_->teammate_seen_message_ids_;
}

std::mutex& TeammateCoordinator::permission_mutex() noexcept {
    return impl_->teammate_permission_mutex_;
}

std::deque<TeammateCoordinator::PendingTeammatePermission>&
TeammateCoordinator::pending_permissions() noexcept {
    return impl_->teammate_pending_permissions_;
}

std::string& TeammateCoordinator::self_agent_id() noexcept {
    return impl_->teammate_self_agent_id_;
}

std::string& TeammateCoordinator::self_agent_name() noexcept {
    return impl_->teammate_self_agent_name_;
}

std::string& TeammateCoordinator::self_team() noexcept {
    return impl_->teammate_self_team_;
}

const std::string& TeammateCoordinator::self_agent_name() const noexcept {
    return impl_->teammate_self_agent_name_;
}

const std::string& TeammateCoordinator::self_team() const noexcept {
    return impl_->teammate_self_team_;
}

std::jthread& TeammateCoordinator::inbox_thread() noexcept {
    return impl_->teammate_inbox_thread_;
}

}  // namespace loom::ui
