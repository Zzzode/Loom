// permission_coordinator.cppm — P4-1d: tool-permission + MCP-elicitation
// blocking-response state extracted from AppAdapter.
//
// Owns the mutex/cv/response triples for the two synchronous dialog
// patterns (tool permission, MCP elicitation) plus the always-allowed
// tool set and the computer-use first-use flag.
//
// AppAdapter keeps the dialog display logic (PushToolPermission /
// PushElicitation + callback wiring) and calls the coordinator's
// reset/wait/resolve methods around the dialog show.

export module loom.ui.app.permission_coordinator;

import std;

export namespace loom::ui {

/// Owns the blocking-response state for tool-permission and MCP-elicitation
/// dialogs.  Both patterns follow the same shape:
///   1. reset_response()          (clear any stale response)
///   2. (show dialog on UI thread)
///   3. wait_response()           (block worker thread)
///   4. resolve(approved)         (UI thread wakes the worker)
///
/// abort_all() unblocks both patterns on shutdown.
class PermissionCoordinator {
public:
    PermissionCoordinator() = default;
    ~PermissionCoordinator() = default;

    // Non-copyable, non-movable (owns mutexes + cvs).
    PermissionCoordinator(const PermissionCoordinator&) = delete;
    PermissionCoordinator& operator=(const PermissionCoordinator&) = delete;

    // ── Tool permission ─────────────────────────────────────────────

    /// Fast-path check: is this tool in the always-allowed set? (thread-safe)
    [[nodiscard]] bool is_always_allowed(std::string_view tool_name) const {
        std::lock_guard lk(permission_mutex_);
        return always_allowed_tools_.contains(std::string(tool_name));
    }

    /// Add a tool to the always-allowed set. (thread-safe)
    void add_always_allowed(std::string tool_name) {
        std::lock_guard lk(permission_mutex_);
        always_allowed_tools_.insert(std::move(tool_name));
    }

    /// Reset the pending permission response.  Must be called before
    /// showing the permission dialog so a stale response doesn't
    /// short-circuit the wait.
    void reset_permission_response() {
        std::lock_guard lk(permission_mutex_);
        permission_response_.reset();
    }

    /// Block until a permission response arrives.  Returns the response
    /// and consumes it (resets).  Called on the worker thread.
    [[nodiscard]] bool wait_permission_response() {
        std::unique_lock lk(permission_mutex_);
        permission_cv_.wait(lk, [this] {
            return permission_response_.has_value();
        });
        bool allowed = *permission_response_;
        permission_response_.reset();
        return allowed;
    }

    /// Resolve a pending permission request.  Called on the UI thread
    /// when the user clicks Allow/Deny (or the dialog is aborted).
    void resolve_permission(bool allowed) {
        std::lock_guard lk(permission_mutex_);
        permission_response_ = allowed;
        permission_cv_.notify_one();
    }

    /// Returns true if this is the first computer-use approval this
    /// session, and marks it as seen.  (caller holds permission_mutex_
    /// via the dialog setup path — but we lock here for safety)
    [[nodiscard]] bool take_computer_use_first_use() {
        std::lock_guard lk(permission_mutex_);
        bool first = !computer_use_seen_in_session_;
        computer_use_seen_in_session_ = true;
        return first;
    }

    // ── MCP elicitation ─────────────────────────────────────────────

    void reset_elicitation_response() {
        std::lock_guard lk(elicitation_mutex_);
        elicitation_response_.reset();
    }

    [[nodiscard]] bool wait_elicitation_response() {
        std::unique_lock lk(elicitation_mutex_);
        elicitation_cv_.wait(lk, [this] {
            return elicitation_response_.has_value();
        });
        bool approved = *elicitation_response_;
        elicitation_response_.reset();
        return approved;
    }

    void resolve_elicitation(bool approved) {
        std::lock_guard lk(elicitation_mutex_);
        elicitation_response_ = approved;
        elicitation_cv_.notify_one();
    }

    // ── Shutdown ────────────────────────────────────────────────────

    /// Abort all pending requests (shutdown).  Sets all responses to
    /// their "denied/cancelled" default and wakes all waiters so
    /// worker threads don't hang.
    void abort_all() {
        {
            std::lock_guard lk(permission_mutex_);
            permission_response_ = false;
        }
        permission_cv_.notify_all();
        {
            std::lock_guard lk(elicitation_mutex_);
            elicitation_response_ = false;
        }
        elicitation_cv_.notify_all();
        {
            std::lock_guard lk(ask_user_mutex_);
            ask_user_response_ = std::optional<std::string>{};
        }
        ask_user_cv_.notify_all();
    }

    // ── Ask-user prompt ─────────────────────────────────────────────

    void reset_ask_user_response() {
        std::lock_guard lk(ask_user_mutex_);
        ask_user_response_.reset();
    }

    /// Block until the user responds.  Returns the inner optional:
    /// std::nullopt = cancelled, std::string = user input.
    [[nodiscard]] std::optional<std::string> wait_ask_user_response() {
        std::unique_lock lk(ask_user_mutex_);
        ask_user_cv_.wait(lk, [this] {
            return ask_user_response_.has_value();
        });
        auto result = *ask_user_response_;
        ask_user_response_.reset();
        return result;
    }

    void resolve_ask_user(std::optional<std::string> value) {
        std::lock_guard lk(ask_user_mutex_);
        ask_user_response_ = std::move(value);
        ask_user_cv_.notify_one();
    }

private:
    // Tool permission state.
    mutable std::mutex permission_mutex_;
    std::condition_variable permission_cv_;
    std::optional<bool> permission_response_;
    std::set<std::string> always_allowed_tools_;
    bool computer_use_seen_in_session_ = false;

    // MCP elicitation state.
    std::mutex elicitation_mutex_;
    std::condition_variable elicitation_cv_;
    std::optional<bool> elicitation_response_;

    // Ask-user prompt state.
    std::mutex ask_user_mutex_;
    std::condition_variable ask_user_cv_;
    std::optional<std::optional<std::string>> ask_user_response_;
};

}  // namespace loom::ui
