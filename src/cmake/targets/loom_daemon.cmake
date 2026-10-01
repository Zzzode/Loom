# ─── loom_daemon: Daemon System ─────────────────────────────────────────────────
add_library(loom_daemon)
target_sources(loom_daemon
    PUBLIC FILE_SET CXX_MODULES FILES
        daemon/daemon_client.cppm
        daemon/daemon_server.cppm
        daemon/worker_registry.cppm
)
target_link_libraries(loom_daemon PUBLIC loom_utils loom_bridge)
