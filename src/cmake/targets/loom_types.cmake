# ─── loom_types: Shared Type Definitions ────────────────────────────────────────
add_library(loom_types)
target_sources(loom_types
    PUBLIC FILE_SET CXX_MODULES FILES
        types/command.cppm
        types/tool_types.cppm
        types/types.cppm
        utils/types/content_array.cppm
        utils/types/tagged_id.cppm
)
target_link_libraries(loom_types PUBLIC loom_utils)
