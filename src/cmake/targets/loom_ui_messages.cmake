# ─── loom_ui_messages: UI message renderers (RFC 0002 F4) ───────────────────
# The cc.ui.messages.* area library: the message row and its per-type
# renderers (assistant / user / tool-use / thinking / error / image /
# attachment / ...), the message pipeline and collapse passes, the
# virtualized message list and scroll keybindings. Split out of the single
# loom_ui target so a body edit in this area recompiles only this area's
# objects (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.messages.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Messages is UI9_RANK 7: it may link lower-ranked
# areas (visual/tools rank 1, foundation rank 2, chrome rank 3,
# widgets rank 5) and never a higher-ranked one.
add_library(loom_ui_messages)
target_sources(loom_ui_messages
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/messages/messages.cppm
        ui/messages/assistant_message.cppm
        ui/messages/error_message.cppm
        ui/messages/message_components.cppm
        ui/messages/message_row.cppm
        ui/messages/message_timestamp.cppm
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
        ui/messages/messages_list.cppm
        ui/messages/message_pipeline.cppm
        ui/messages/collapse_background_bash.cppm
        # RFC 0001 Phase D B5a: moved from loom_utils (src/utils/messages/).
        # Pure leaves (import std only); test-only importers.
        ui/messages/collapse_notifications.cppm
        ui/messages/collapse_read_search.cppm
        ui/messages/message_predicates.cppm
        ui/messages/scroll_keybindings.cppm
        ui/messages/virtual_message_list.cppm
)
# Module implementation units for cc.ui.messages.messages_list
# (RFC 0001 Phase C batch 7): filter/brief logic, the ~20-module
# std::visit search closure (isolated on purpose), row geometry,
# envelope/divider chrome, the heavy payload_row faithful-dispatch TU,
# static/virtual view builders, and the component vtable anchor. Moved
# from loom_ui unchanged; they implement the module interface owned by
# this target.
target_sources(loom_ui_messages PRIVATE
    ui/messages/messages_list_filter.cpp
    ui/messages/messages_list_search.cpp
    ui/messages/messages_list_geometry.cpp
    ui/messages/messages_list_envelope.cpp
    ui/messages/messages_list_payload_row.cpp
    ui/messages/messages_list_view.cpp
    ui/messages/messages_list_component.cpp
)
# cc.ui.foundation.* (design_tokens / design_figures / theme_provider),
# cc.ui.visual.* (code_highlight / markdown), cc.ui.tools.* (registry /
# generic), cc.ui.chrome.* (layout / ansi_render), and
# cc.ui.widgets.spinner. External deps: cc.types.types, cc.utils.*
# (image_store / hyperlink), and FTXUI (component / dom / screen headers).
# loom_std's `import std;` BMI arrives via the directory-level
# link_libraries(loom_std). Over-linking is safe (and matches the previous
# loom_ui.cmake behaviour).
target_link_libraries(loom_ui_messages
    PUBLIC
        loom_ui_foundation
        loom_ui_visual
        loom_ui_tools
        loom_ui_chrome
        loom_ui_widgets
        loom_types
        loom_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
