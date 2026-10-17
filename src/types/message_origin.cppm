export module loom.types.message_origin;

import std;

export namespace loom::core {

struct HumanInput {
    bool operator==(const HumanInput&) const = default;
};

// An old persisted message whose author cannot be inferred from its text.
struct LegacyHistory {
    bool operator==(const LegacyHistory&) const = default;
};

enum class TaskEventKind { Subagent, TeamAgent };
enum class TaskEventStatus { Completed, Failed, Stopped };

// UI/routing facts only. Model results and paths remain in message content.
struct TaskEvent {
    std::string task_id;
    std::string parent_session_id;
    TaskEventKind kind = TaskEventKind::Subagent;
    TaskEventStatus status = TaskEventStatus::Completed;
    std::string display_name;

    bool operator==(const TaskEvent&) const = default;
};

using UserMessageOrigin = std::variant<HumanInput, LegacyHistory, TaskEvent>;

} // namespace loom::core
