# ─── loom_bootstrap: Startup Bootstrap ─────────────────────────────────────────
add_library(loom_bootstrap)
target_sources(loom_bootstrap
    PUBLIC FILE_SET CXX_MODULES FILES
        bootstrap/interactive_helpers.cppm
)
# RFC-0001 B15: mcp_connectivity moved to loom_orchestration, taking the only
# loom_tools edge with it. interactive_helpers imports loom.hooks.ide_at_mentioned
# and loom.serdes.json only.
target_link_libraries(loom_bootstrap PUBLIC loom_utils loom_state loom_config loom_services loom_hooks)
