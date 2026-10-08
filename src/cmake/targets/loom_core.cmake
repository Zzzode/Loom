# ─── loom_core: Aggregator (links ALL sub-libraries) ────────────────────────────
# Convenience INTERFACE library that links all subsystems together.
add_library(loom_core INTERFACE)
target_link_libraries(loom_core
    INTERFACE
        loom_utils
        loom_state
        loom_constants
        loom_types
        loom_config
        loom_keybindings
        loom_memdir
        loom_coordinator
        loom_query
        loom_session
        loom_bootstrap
        loom_tasks
        loom_tools
        # RFC-0001 B15: test_fix_lsp_tool links loom_core ONLY and imports
        # loom.orchestration.tools.lsp; the aggregator must carry it.
        loom_orchestration
        loom_commands
        loom_services
        loom_hooks
        loom_cli
        loom_bridge
        loom_skills_core
        loom_skills
        loom_ui
        loom_vim
        loom_daemon
        loom_server
        loom_benchmarks
        loom_plugins
        loom_task_types
        loom_history
        CURL::libcurl
        yyjson
        uv_a
)
