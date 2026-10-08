# ─── loom_tasks: Task System ────────────────────────────────────────────────────
add_library(loom_tasks)
target_sources(loom_tasks
    PUBLIC FILE_SET CXX_MODULES FILES
        tasks/in_process_teammate_task.cppm
        tasks/local_agent_task.cppm
        tasks/pill_label.cppm
        tasks/task.cppm
        tasks/task_graph.cppm
        tasks/task_utils.cppm
        tasks/types.cppm
)
target_link_libraries(loom_tasks PUBLIC loom_utils loom_types loom_state loom_coordinator loom_hooks loom_constants)
