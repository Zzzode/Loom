module loom.types.message_origin_json;

import std;

namespace loom::core {
namespace {

bool valid_event_text(std::string_view value, bool identity) noexcept {
    if (value.empty() || value.size() > 4096) return false;
    for (std::size_t index = 0; index < value.size();) {
        const auto first = static_cast<unsigned char>(value[index++]);
        if (first < 0x80) {
            if (identity && (first < 0x20 || first == 0x7f)) return false;
            continue;
        }
        unsigned remaining;
        std::uint32_t point;
        std::uint32_t minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            remaining = 1; point = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            remaining = 2; point = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            remaining = 3; point = first & 0x07; minimum = 0x10000;
        } else {
            return false;
        }
        if (value.size() - index < remaining) return false;
        while (remaining-- > 0) {
            const auto next = static_cast<unsigned char>(value[index++]);
            if ((next & 0xc0) != 0x80) return false;
            point = (point << 6) | (next & 0x3f);
        }
        if (point < minimum || point > 0x10ffff ||
            (point >= 0xd800 && point <= 0xdfff)) return false;
    }
    return true;
}

} // namespace

bool is_valid_task_event(const TaskEvent& event) noexcept {
    const bool known_kind = event.kind == TaskEventKind::Subagent ||
        event.kind == TaskEventKind::TeamAgent;
    const bool known_status = event.status == TaskEventStatus::Completed ||
        event.status == TaskEventStatus::Failed || event.status == TaskEventStatus::Stopped;
    return known_kind && known_status && valid_event_text(event.task_id, true) &&
        valid_event_text(event.parent_session_id, true) &&
        valid_event_text(event.display_name, false);
}

void write_user_message_origin(const UserMessageOrigin& origin,
    loom::utils::json::JsonMutVal& object, loom::utils::json::JsonMutDoc& doc) {
    if (std::holds_alternative<LegacyHistory>(origin)) {
        object.add("origin", doc.string("legacy_history"));
        return;
    }
    const auto* event = std::get_if<TaskEvent>(&origin);
    if (!event || !is_valid_task_event(*event)) {
        object.add("origin", doc.string("human"));
        return;
    }
    object.add("origin", doc.string("task_event"));
    auto value = doc.object();
    value.add("version", doc.number(static_cast<std::int64_t>(1)));
    value.add("task_id", doc.string(event->task_id));
    value.add("parent_session_id", doc.string(event->parent_session_id));
    value.add("kind", doc.string(event->kind == TaskEventKind::Subagent ? "subagent" : "team_agent"));
    const auto status = event->status == TaskEventStatus::Completed ? "completed" :
        event->status == TaskEventStatus::Failed ? "failed" : "stopped";
    value.add("status", doc.string(status));
    value.add("display_name", doc.string(event->display_name));
    object.add("task_event", value);
}

UserMessageOrigin read_user_message_origin(loom::utils::json::JsonVal object) {
    if (!object.is_obj()) return HumanInput{};
    const auto origin = object.get("origin");
    const auto value = object.get("task_event");
    if (!origin.valid() && !value.valid()) return LegacyHistory{};
    if (!origin.is_str()) return HumanInput{};
    if (origin.as_str() == "human") return HumanInput{};
    if (origin.as_str() == "legacy_history") {
        return value.valid() ? UserMessageOrigin{HumanInput{}} : UserMessageOrigin{LegacyHistory{}};
    }
    if (origin.as_str() != "task_event" || !value.is_obj()) return HumanInput{};
    const auto version = value.get("version");
    if (!version.is_int() || version.as_int() != 1) return HumanInput{};
    const auto task_id = value.get("task_id");
    const auto parent = value.get("parent_session_id");
    const auto kind = value.get("kind");
    const auto status = value.get("status");
    const auto name = value.get("display_name");
    if (!task_id.is_str() || !parent.is_str() || !kind.is_str() ||
        !status.is_str() || !name.is_str()) return HumanInput{};
    TaskEvent event;
    event.task_id = task_id.as_str();
    event.parent_session_id = parent.as_str();
    event.display_name = name.as_str();
    if (kind.as_str() == "subagent") event.kind = TaskEventKind::Subagent;
    else if (kind.as_str() == "team_agent") event.kind = TaskEventKind::TeamAgent;
    else return HumanInput{};
    if (status.as_str() == "completed") event.status = TaskEventStatus::Completed;
    else if (status.as_str() == "failed") event.status = TaskEventStatus::Failed;
    else if (status.as_str() == "stopped") event.status = TaskEventStatus::Stopped;
    else return HumanInput{};
    return is_valid_task_event(event) ? UserMessageOrigin{std::move(event)} :
        UserMessageOrigin{HumanInput{}};
}

} // namespace loom::core
