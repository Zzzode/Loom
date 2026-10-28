export module loom.types.message_origin_json;

import loom.types.message_origin;
import loom.serdes.json;

export namespace loom::core {

[[nodiscard]] bool is_valid_task_event(const TaskEvent& event) noexcept;
void write_user_message_origin(const UserMessageOrigin& origin,
                               loom::utils::json::JsonMutVal& object,
                               loom::utils::json::JsonMutDoc& doc);
[[nodiscard]] UserMessageOrigin read_user_message_origin(loom::utils::json::JsonVal object);

}  // namespace loom::core
