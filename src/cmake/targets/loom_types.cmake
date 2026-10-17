# ─── loom_types: Shared Type Definitions ────────────────────────────────────────
add_library(loom_types)
target_sources(loom_types
    PUBLIC FILE_SET CXX_MODULES FILES
        types/command.cppm
        types/tool_types.cppm
        types/types.cppm
        types/message_origin.cppm
        types/message_origin_json.cppm
        utils/types/content_array.cppm
        utils/types/tagged_id.cppm
)
target_sources(loom_types PRIVATE types/message_origin_json.cpp)
target_link_libraries(loom_types PUBLIC loom_utils)
