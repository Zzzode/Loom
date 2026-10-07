# ─── loom_ui_messages_renderers: per-type message renderers ─────────────────
# The cc.ui.messages.* renderers sub-library: the 24 per-type faithful
# renderers (assistant / user / tool-use / thinking / error / image /
# attachment / ...).  Split from loom_ui_messages (P2-2a) so a body edit
# in one renderer recompiles only that renderer's object, not the
# framework closure.
#
# Messages is UI9_RANK 7: this sub-library may link lower-ranked areas
# and loom_ui_messages_core (same area, no cycle).
add_library(loom_ui_messages_renderers)
target_sources(loom_ui_messages_renderers
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/messages/assistant_message.cppm
        ui/messages/error_message.cppm
        ui/messages/message_advisor.cppm
        ui/messages/message_bash_io.cppm
        ui/messages/message_channel.cppm
        ui/messages/message_hook_progress.cppm
        ui/messages/message_plan_approval.cppm
        ui/messages/message_rate_limit.cppm
        ui/messages/message_task_assignment.cppm
        ui/messages/message_tool_result.cppm
        ui/messages/thinking_message.cppm
        ui/messages/tool_use_message.cppm
        ui/messages/user_message.cppm
        ui/messages/message_image.cppm
        ui/messages/message_compact_boundary.cppm
        ui/messages/message_shutdown.cppm
        ui/messages/message_user_command.cppm
        ui/messages/user_text_message.cppm
        ui/messages/assistant_text_message.cppm
        ui/messages/system_text_message.cppm
        ui/messages/attachment_message.cppm
        ui/messages/api_error_message.cppm
        ui/messages/collapsed_content_message.cppm
        ui/messages/local_command_output_message.cppm
)
target_link_libraries(loom_ui_messages_renderers
    PUBLIC
        loom_ui_messages_core
        loom_ui_foundation
        loom_ui_visual
        loom_ui_chrome
        loom_ui_widgets
        loom_types
        loom_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
