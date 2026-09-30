# ─── cc_bootstrap: Startup Bootstrap ─────────────────────────────────────────
add_library(cc_bootstrap)
target_sources(cc_bootstrap
    PUBLIC FILE_SET CXX_MODULES FILES
        bootstrap/interactive_helpers.cppm
)
# RFC-0001 B15: mcp_connectivity moved to cc_orchestration, taking the only
# cc_tools edge with it. interactive_helpers imports cc.hooks.ide_at_mentioned
# and cc.serdes.json only.
target_link_libraries(cc_bootstrap PUBLIC cc_utils cc_state cc_config cc_services cc_hooks)
