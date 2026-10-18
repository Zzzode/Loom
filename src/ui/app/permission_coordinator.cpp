module loom.ui.app.permission_coordinator;

import std;

namespace loom::ui {

bool PermissionCoordinator::is_always_allowed(std::string_view tool_name) const {
    std::lock_guard lock(permission_mutex_);
    return !stopping_.load() && always_allowed_tools_.contains(std::string(tool_name));
}

void PermissionCoordinator::add_always_allowed(std::string tool_name) {
    std::lock_guard lock(permission_mutex_);
    if (!stopping_.load()) always_allowed_tools_.insert(std::move(tool_name));
}

bool PermissionCoordinator::take_computer_use_first_use() {
    std::lock_guard lock(permission_mutex_);
    const bool first = !computer_use_seen_in_session_;
    computer_use_seen_in_session_ = true;
    return first;
}

PermissionCoordinator::RequestToken PermissionCoordinator::begin_permission_request() {
    std::unique_lock lock(permission_mutex_);
    permission_cv_.wait(lock, [this] { return stopping_.load() || permission_active_ == 0; });
    if (stopping_.load()) return 0;
    permission_response_.reset();
    permission_active_ = ++permission_next_;
    return permission_active_;
}

bool PermissionCoordinator::permission_request_current(RequestToken token) const {
    std::lock_guard lock(permission_mutex_);
    return token != 0 && token == permission_active_ && !stopping_.load();
}

bool PermissionCoordinator::wait_permission_response(RequestToken token) {
    std::unique_lock lock(permission_mutex_);
    permission_cv_.wait(lock, [this, token] {
        return stopping_.load() || token == 0 || token != permission_active_ ||
               permission_response_.has_value();
    });
    return !stopping_.load() && token != 0 && token == permission_active_ &&
           permission_response_.value_or(false);
}

bool PermissionCoordinator::resolve_permission(RequestToken token, bool allowed,
                                                std::string_view always_allow_tool) {
    std::lock_guard lock(permission_mutex_);
    if (stopping_.load() || token == 0 || token != permission_active_ ||
        permission_response_.has_value()) return false;
    if (allowed && !always_allow_tool.empty()) {
        always_allowed_tools_.insert(std::string(always_allow_tool));
    }
    permission_response_ = allowed;
    permission_cv_.notify_all();
    return true;
}

void PermissionCoordinator::finish_permission_request(RequestToken token) {
    std::lock_guard lock(permission_mutex_);
    if (token == 0 || token != permission_active_) return;
    permission_active_ = 0;
    permission_response_.reset();
    permission_cv_.notify_all();
}

PermissionCoordinator::RequestToken PermissionCoordinator::begin_elicitation_request() {
    std::unique_lock lock(elicitation_mutex_);
    elicitation_cv_.wait(lock, [this] { return stopping_.load() || elicitation_active_ == 0; });
    if (stopping_.load()) return 0;
    elicitation_response_.reset();
    elicitation_active_ = ++elicitation_next_;
    return elicitation_active_;
}

bool PermissionCoordinator::elicitation_request_current(RequestToken token) const {
    std::lock_guard lock(elicitation_mutex_);
    return token != 0 && token == elicitation_active_ && !stopping_.load();
}

bool PermissionCoordinator::wait_elicitation_response(RequestToken token) {
    std::unique_lock lock(elicitation_mutex_);
    elicitation_cv_.wait(lock, [this, token] {
        return stopping_.load() || token == 0 || token != elicitation_active_ ||
               elicitation_response_.has_value();
    });
    return !stopping_.load() && token != 0 && token == elicitation_active_ &&
           elicitation_response_.value_or(false);
}

bool PermissionCoordinator::resolve_elicitation(RequestToken token, bool approved) {
    std::lock_guard lock(elicitation_mutex_);
    if (stopping_.load() || token == 0 || token != elicitation_active_ ||
        elicitation_response_.has_value()) return false;
    elicitation_response_ = approved;
    elicitation_cv_.notify_all();
    return true;
}

void PermissionCoordinator::finish_elicitation_request(RequestToken token) {
    std::lock_guard lock(elicitation_mutex_);
    if (token == 0 || token != elicitation_active_) return;
    elicitation_active_ = 0;
    elicitation_response_.reset();
    elicitation_cv_.notify_all();
}

void PermissionCoordinator::abort_all() {
    stopping_.store(true);
    // Take each slot lock so an entering wait cannot miss the shutdown wake.
    { std::lock_guard lock(permission_mutex_); }
    permission_cv_.notify_all();
    { std::lock_guard lock(elicitation_mutex_); }
    elicitation_cv_.notify_all();
    { std::lock_guard lock(ask_user_mutex_); }
    ask_user_cv_.notify_all();
}

void PermissionCoordinator::reset_ask_user_response() {
    std::lock_guard lock(ask_user_mutex_);
    if (!stopping_.load()) ask_user_response_.reset();
}

std::optional<std::string> PermissionCoordinator::wait_ask_user_response() {
    std::unique_lock lock(ask_user_mutex_);
    ask_user_cv_.wait(lock, [this] {
        return stopping_.load() || ask_user_response_.has_value();
    });
    if (stopping_.load()) return std::nullopt;
    auto result = *ask_user_response_;
    ask_user_response_.reset();
    return result;
}

void PermissionCoordinator::resolve_ask_user(std::optional<std::string> value) {
    std::lock_guard lock(ask_user_mutex_);
    if (stopping_.load()) return;
    ask_user_response_ = std::move(value);
    ask_user_cv_.notify_one();
}

}  // namespace loom::ui
