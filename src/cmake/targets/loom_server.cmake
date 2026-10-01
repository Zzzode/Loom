# ─── loom_server: Server ────────────────────────────────────────────────────────
add_library(loom_server)
target_sources(loom_server
    PUBLIC FILE_SET CXX_MODULES FILES
        server/control_protocol.cppm
        server/server_main.cppm
        server/server_routes.cppm
        server/types.cppm
    PRIVATE
        server/control_protocol_serde.cpp
)
# RFC-0001 cc-sdk phase 3 (§2.1): loom_query is PUBLIC — server_main.cppm and
# the test_services TU import cc.server.server_routes, whose module interface
# imports cc.query.assembly (the extracted engine recipe); consumers need its
# BMI on their compile line. loom_orchestration stays PUBLIC because
# cc.query.assembly's BMI imports cc.orchestration.runtime_backends (also
# transitive via loom_query, linked direct for BMI propagation).
target_link_libraries(loom_server PUBLIC
    loom_utils
    loom_session
    loom_query
    loom_tools
    loom_orchestration
    OpenSSL::Crypto)
