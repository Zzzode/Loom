# ─── loom_ui_messages_core: shared message types and predicates ─────────────
# The cc.ui.messages.* core sub-library: shared row components, timestamp
# formatting, row predicates, and XML-like message parsing. Split from
# loom_ui_messages (P2-2a) so a renderer .cppm edit
# no longer cascades through the core BMI.
#
# Messages is UI9_RANK 7: this sub-library may link lower-ranked areas
# (foundation rank 2, visual rank 1, chrome rank 3, widgets rank 5)
# and never a higher-ranked one.
add_library(loom_ui_messages_core)
target_sources(loom_ui_messages_core
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/messages/message_components.cppm
        ui/messages/message_timestamp.cppm
        ui/messages/message_predicates.cppm
        ui/messages/xml_tags.cppm
)
target_link_libraries(loom_ui_messages_core
    PUBLIC
        loom_ui_foundation
        loom_constants
        loom_types
        loom_utils
        ftxui::dom
)
