# ─── cc_types: Shared Type Definitions ────────────────────────────────────────
add_library(cc_types)
target_sources(cc_types
    PUBLIC FILE_SET CXX_MODULES FILES
        types/command.cppm
        types/tool_types.cppm
        types/types.cppm
        utils/types/content_array.cppm
        utils/types/tagged_id.cppm
)
target_link_libraries(cc_types PUBLIC cc_utils)
