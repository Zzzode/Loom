# ─── loom_ui_messages: UI message framework (RFC 0002 F4, P2-2a split) ──────
# The cc.ui.messages.* framework sub-library: the message row dispatch hub,
# the messages list + virtual list, the message pipeline and collapse passes,
# and scroll keybindings.  Split from the former monolithic target so a
# renderer .cppm edit no longer cascades through the framework BMI.
#
# Grouped by MODULE-NAME area (export module cc.ui.messages.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Messages is UI9_RANK 7: it may link lower-ranked
# areas (visual/tools rank 1, foundation rank 2, chrome rank 3,
# widgets rank 5) and loom_ui_messages_renderers (same area, no cycle).
add_library(loom_ui_messages)
target_sources(loom_ui_messages
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/messages/message_row.cppm
        ui/messages/messages_list.cppm
        ui/messages/message_pipeline.cppm
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
#
# P2-2b: message_row_dispatch.cpp is the impl unit for
# cc.ui.messages.message_row — the 228-line RenderMessageRowByType body
# moved out of the .cppm BMI.
target_sources(loom_ui_messages PRIVATE
    ui/messages/message_row_dispatch.cpp
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
        loom_ui_messages_renderers
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
