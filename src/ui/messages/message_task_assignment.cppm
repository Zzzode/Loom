/// @file message_task_assignment.cppm
/// @brief Task assignment message rendering for multi-agent
module;

#include <ftxui/dom/elements.hpp>

export module loom.ui.messages.message_task_assignment;

import std;

import loom.ui.foundation.theme_provider;

export namespace loom::ui::messages {

using namespace ftxui;

/// Task assignment status
enum class TaskAssignmentStatus {
    Assigned,
    InProgress,
    Completed,
    Failed,
    Cancelled,
};

/// Task assignment display options
struct TaskAssignmentOptions {
    std::string task_id;
    std::string description;
    std::string assignee;
    TaskAssignmentStatus status{TaskAssignmentStatus::Assigned};
    std::optional<std::string> agent_color;
};

/// Render task assignment message
[[nodiscard]] inline Element render_task_assignment(const TaskAssignmentOptions& opts) {
    auto status_text = [&]() -> std::pair<std::string, Color> {
        switch (opts.status) {
        case TaskAssignmentStatus::Assigned:
            return {"ASSIGNED", loom::ui::design::theme::current_theme().palette->info};
        case TaskAssignmentStatus::InProgress:
            return {"IN PROGRESS", loom::ui::design::theme::current_theme().palette->warning};
        case TaskAssignmentStatus::Completed:
            return {"DONE", loom::ui::design::theme::current_theme().palette->success};
        case TaskAssignmentStatus::Failed:
            return {"FAILED", loom::ui::design::theme::current_theme().palette->danger};
        case TaskAssignmentStatus::Cancelled:
            return {"CANCELLED", loom::ui::design::theme::current_theme().palette->muted};
        }
        return {"UNKNOWN", loom::ui::design::theme::current_theme().palette->text};
    }();

    return hbox({
        text("[" + status_text.first + "]") | color(status_text.second) | bold,
        text(" "),
        text(opts.assignee) | bold,
        text(": "),
        text(opts.description),
    });
}

}  // namespace loom::ui::messages
