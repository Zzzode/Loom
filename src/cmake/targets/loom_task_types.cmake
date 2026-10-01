# ─── loom_task_types: Canonical Task Data Model ─────────────────────────────────
add_library(loom_task_types)
target_sources(loom_task_types
    PUBLIC FILE_SET CXX_MODULES FILES
        types/task_types.cppm
)
target_link_libraries(loom_task_types PUBLIC loom_utils)
