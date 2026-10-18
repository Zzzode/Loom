// Request-bound blocking responses for application-level permission overlays.
export module loom.ui.app.permission_coordinator;

import std;

export namespace loom::ui {

class PermissionCoordinator {
public:
    using RequestToken = std::uint64_t;
    PermissionCoordinator() = default;
    ~PermissionCoordinator() = default;
    PermissionCoordinator(const PermissionCoordinator&) = delete;
    PermissionCoordinator& operator=(const PermissionCoordinator&) = delete;

    [[nodiscard]] bool is_always_allowed(std::string_view tool_name) const;
    void add_always_allowed(std::string tool_name);
    [[nodiscard]] bool take_computer_use_first_use();

    // A request owns its kind's slot through exact-ID UI cleanup, not just
    // through the response. Zero means shutdown rejected a queued caller.
    [[nodiscard]] RequestToken begin_permission_request();
    [[nodiscard]] bool permission_request_current(RequestToken token) const;
    [[nodiscard]] bool wait_permission_response(RequestToken token);
    [[nodiscard]] bool resolve_permission(RequestToken token, bool allowed,
                                         std::string_view always_allow_tool = {});
    void finish_permission_request(RequestToken token);

    [[nodiscard]] RequestToken begin_elicitation_request();
    [[nodiscard]] bool elicitation_request_current(RequestToken token) const;
    [[nodiscard]] bool wait_elicitation_response(RequestToken token);
    [[nodiscard]] bool resolve_elicitation(RequestToken token, bool approved);
    void finish_elicitation_request(RequestToken token);

    // Sticky shutdown denies active and queued requests; selected-Agent Stop
    // must never call this application-lifetime operation.
    void abort_all();

    // Native Agent AskUser remains prohibited; this root-only slot is unchanged.
    void reset_ask_user_response();
    [[nodiscard]] std::optional<std::string> wait_ask_user_response();
    void resolve_ask_user(std::optional<std::string> value);

private:
    std::atomic<bool> stopping_{false};
    mutable std::mutex permission_mutex_;
    std::condition_variable permission_cv_;
    RequestToken permission_next_ = 0;
    RequestToken permission_active_ = 0;
    std::optional<bool> permission_response_;
    std::set<std::string> always_allowed_tools_;
    bool computer_use_seen_in_session_ = false;

    mutable std::mutex elicitation_mutex_;
    std::condition_variable elicitation_cv_;
    RequestToken elicitation_next_ = 0;
    RequestToken elicitation_active_ = 0;
    std::optional<bool> elicitation_response_;

    std::mutex ask_user_mutex_;
    std::condition_variable ask_user_cv_;
    std::optional<std::optional<std::string>> ask_user_response_;
};

}  // namespace loom::ui
