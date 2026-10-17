/// Session-scoped runtime instances; separate from agent-definition cards.
module;

#include <ftxui/dom/elements.hpp>

export module loom.ui.features.agents.runtime_agents;

import std;
import loom.ui.features.todos.todo_panel;
import loom.ui.features.teams.live_teammates;

export namespace loom::ui::agents::runtime {

enum class Status { Queued, Running, Completed, Failed, Cancelled };

struct Instance {
    std::string agent_id;
    std::string name;
    std::string description;
    Status status = Status::Queued;
    bool background = false;

    bool operator==(const Instance&) const = default;
};

[[nodiscard]] bool is_active(Status status) noexcept;
[[nodiscard]] std::string_view status_name(Status status) noexcept;

/// Pure cell-bounded renderer. Tasks, Subagents and Agent Teams have separate
/// headings and indented children within the caller-supplied row budget.
/// The caller projects session-owned data; rendering never changes its order.
[[nodiscard]] ftxui::Element RenderRuntimeAgentRow(const Instance& instance, int columns);
[[nodiscard]] ftxui::Element RenderRuntimeAgentActivity(
    const std::vector<Instance>& instances,
    const std::vector<loom::ui::todos::TodoDisplayItem>& todos,
    const std::vector<loom::ui::teams::live::LiveTeammate>& teams,
    int columns, int budget, bool todos_collapsed = false,
    std::string_view response_prefix = {});

} // namespace loom::ui::agents::runtime
