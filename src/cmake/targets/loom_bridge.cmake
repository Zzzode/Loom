# ─── loom_bridge: IDE Bridge ────────────────────────────────────────────────────
add_library(loom_bridge)
target_sources(loom_bridge
    PUBLIC FILE_SET CXX_MODULES FILES
        bridge/api.cppm
        bridge/bridge_messaging.cppm
        bridge/config.cppm
        bridge/core.cppm
        bridge/debug_utils.cppm
        bridge/flush_gate.cppm
        bridge/jwt_utils.cppm
        bridge/messages.cppm
        bridge/security.cppm
        bridge/session_api.cppm
        bridge/session_id_compat.cppm
        bridge/transport.cppm
        bridge/work_secret.cppm
)
target_link_libraries(loom_bridge
    PUBLIC
        loom_utils
        loom_types
        loom_cli
        loom_server
        OpenSSL::Crypto
        yyjson
        uv_a
)
