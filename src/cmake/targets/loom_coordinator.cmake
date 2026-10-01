# ─── loom_coordinator: Multi-Agent Orchestration ────────────────────────────────
add_library(loom_coordinator)
target_sources(loom_coordinator
    PUBLIC FILE_SET CXX_MODULES FILES
        coordinator/coordinator_types.cppm
        coordinator/swarm.cppm
)
target_link_libraries(loom_coordinator PUBLIC loom_utils loom_state)
