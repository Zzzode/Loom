// task_view_store.cppm — RFC 0002 F3 store: task-view state (spinner mode,
// task notifications, agent/teammate live state), sharded out of
// ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). features (8) is downward-legal, so the concrete
// by-value field types (AgentCardData, LiveTeammate) recreate no up-edge.
module;

export module loom.ui.screens.task_view_store;

import std;

import loom.ui.features.agents.agent_cards;        // AgentCardData
import loom.ui.features.teams.live_teammates;      // LiveTeammate

export namespace loom::ui::repl_screen {

/// Spinner modes.  TS SpinnerMode (SpinnerAnimationRow.tsx switch cases +
/// SpinnerWithVerb usage in REPL.tsx): requesting/thinking/responding/
/// tool-input/tool-use + briefing/idle for KAIROS brief mode.
/// Moved here from repl_state.cppm in RFC 0002 F3 (TaskViewStore shard).
enum class SpinnerMode : std::uint8_t {
    Hidden, Requesting, Thinking, Responding,
    ToolInput, ToolUse, Briefing, IdleBrief,
};

/// RFC 0002 F3 store — task-view state (spinner mode, task notifications,
/// agent/teammate live state), sharded out of ReplScreenState. Homed in
/// loom.ui.screens (rank 10): the concrete cross-area field types
/// (AgentCardData, LiveTeammate) live in features (rank 8), so by-value
/// fields recreate no up-edge. UI-thread-affined plain data — see the
/// file header for the threading and import rules.
struct TaskViewStore {
    // ── Spinner ─────────────────────────────────────────────────────────
    SpinnerMode spinner_mode = SpinnerMode::Hidden;
    std::optional<std::string> spinner_verb, spinner_tip;

    // ── Task notifications / footer counts ──────────────────────────────
    // Background-task and teammate counts for the footer mode indicator
    // (prompt_input_footer ModeIndicatorOpts). teams_footer_selected is
    // the footer focus state for the teams item.
    int background_task_count = 0, teammate_count = 0;
    bool teams_footer_selected = false;

    // ── Live teams (leader view) ────────────────────────────────────────
    // TS REF: src/components/teams/TeamStatus.tsx (footer count) +
    // TeamsDialog.tsx (roster) + CoordinatorAgentStatus.tsx AgentLine
    // (per-teammate live status + output tail). Projected by AppAdapter from
    // (a) agent_runtime::native_agent_store() for in-process teammates and
    // (b) loom::utils::swarm_pane_observer for tmux pane teammates.
    // Event-driven: the observer posts a refresh when pane content changes;
    // there is no render ticker (see app_team_projection.cpp).
    std::vector<teams::live::LiveTeammate> live_teammates;
    // Selection cursor for the TeamsView modal (TeamsViewPayload.selected_index
    // mirrors this when the dialog is open).
    int teams_overview_selected_index = 0;

    // ── Agent cards (AgentsView / agent menu) ───────────────────────────
    // Projected agent-definition cards for the AgentsView modal and the
    // agent menu. Populated by AppAdapter from the agent definitions.
    std::vector<loom::ui::agents::cards::AgentCardData> agent_cards;
};

}  // namespace loom::ui
