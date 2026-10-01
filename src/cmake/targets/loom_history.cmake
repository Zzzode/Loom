# ─── loom_history: Canonical Conversation History ────────────────────────────────
add_library(loom_history)
target_sources(loom_history
    PUBLIC FILE_SET CXX_MODULES FILES
        types/history.cppm
)
target_link_libraries(loom_history PUBLIC loom_utils)
