// teammate_coordinator.cppm — P3-1a: teammate inbox/permission state
// extracted from AppAdapter.
//
// Owns the TeammateState PIMPL (inbox worker thread, pending prompts,
// pending permissions, self-agent identity) and the self-contained
// methods that only touch that state.  AppAdapter keeps the methods that
// need engine/screen_state/dialog context (inbox polling, live-teammate
// projection, permission draining) and accesses the coordinator's state
// through the accessors below.

export module loom.ui.app.teammate_coordinator;

import std;

import loom.teams.swarm.helpers;  // SwarmPermissionRequestMessage

export namespace loom::ui {

/// Owns the teammate inbox/permission cluster.  The state is a PIMPL so
/// the swarm/observer import closure stays out of app.cppm's BMI.
class TeammateCoordinator {
public:
    TeammateCoordinator();
    ~TeammateCoordinator();

    // Non-copyable, non-movable (owns a jthread).
    TeammateCoordinator(const TeammateCoordinator&) = delete;
    TeammateCoordinator& operator=(const TeammateCoordinator&) = delete;

    // ── Self-contained methods (no AppAdapter context needed) ──────────

    /// True when this process was spawned with teammate identity.
    [[nodiscard]] bool running_as_pane_teammate() const;

    /// Configure teammate identity (testing seam).
    void configure_for_testing(std::string agent_name, std::string team);

    /// Number of pending teammate prompts (testing seam).
    [[nodiscard]] std::size_t pending_prompt_count() const;

    /// Pop one pending teammate prompt (testing seam).
    [[nodiscard]] std::string pop_prompt_for_testing();

    /// Number of pending teammate permissions (testing seam).
    [[nodiscard]] std::size_t pending_permission_count() const;

    // ── State accessors (for AppAdapter methods that need engine /
    //    screen_state / dialog context) ────────────────────────────────

    struct PendingTeammatePermission {
        loom::utils::swarm_helpers::SwarmPermissionRequestMessage request;
        std::string team;
    };

    /// Access the inbox mutex + pending prompts.  Callers must hold the
    /// returned mutex's lock while touching the deque.
    [[nodiscard]] std::mutex& pending_mutex() noexcept;
    [[nodiscard]] std::deque<std::string>& pending_prompts() noexcept;
    [[nodiscard]] std::unordered_set<std::string>& seen_message_ids() noexcept;

    /// Access the permission mutex + pending permissions.
    [[nodiscard]] std::mutex& permission_mutex() noexcept;
    [[nodiscard]] std::deque<PendingTeammatePermission>& pending_permissions() noexcept;

    /// Self-agent identity.
    [[nodiscard]] std::string& self_agent_id() noexcept;
    [[nodiscard]] std::string& self_agent_name() noexcept;
    [[nodiscard]] std::string& self_team() noexcept;
    [[nodiscard]] const std::string& self_agent_name() const noexcept;
    [[nodiscard]] const std::string& self_team() const noexcept;

    /// The inbox worker jthread.
    [[nodiscard]] std::jthread& inbox_thread() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace loom::ui
