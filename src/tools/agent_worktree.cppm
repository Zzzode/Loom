/// @file agent_worktree.cppm
/// @brief Agent worktree cleanup leaf (RFC-0001 B14 / F14 Alpha 2).
///
/// The cleanup entry point used to live on the cc.tools.agent facade
/// (declared in cc.tools.agent.utils, defined in its hooks impl unit). Team
/// teardown (runtime_team_shared) needs ONLY this function off the agent
/// subtree, so it is parked on this zero-facade leaf: the team module can
/// import cc.tools.agent_worktree without an edge back into the facade that
/// B15 lifts into cc.orchestration.
///
/// Worktree CREATION stays in agent.utils (it consumes AgentExecutionPlan).
export module cc.tools.agent_worktree;

import std;

export namespace cc::tools::agent {

struct AgentWorktreeCleanupResult {
    bool attempted = false;
    bool removed = false;
    bool changed = false;
    std::string message;
};

[[nodiscard]] AgentWorktreeCleanupResult cleanup_agent_worktree(std::string_view agent_id);

} // namespace cc::tools::agent
