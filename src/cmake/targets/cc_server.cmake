# ─── cc_server: Server ────────────────────────────────────────────────────────
add_library(cc_server)
target_sources(cc_server
    PUBLIC FILE_SET CXX_MODULES FILES
        server/control_protocol.cppm
        server/server_main.cppm
        server/server_routes.cppm
        server/types.cppm
)
# RFC-0001 cc-sdk phase 3 (§2.1): cc_query is PUBLIC — server_main.cppm and
# the test_services TU import cc.server.server_routes, whose module interface
# imports cc.query.assembly (the extracted engine recipe); consumers need its
# BMI on their compile line. cc_orchestration stays PUBLIC because
# cc.query.assembly's BMI imports cc.orchestration.runtime_backends (also
# transitive via cc_query, linked direct for BMI propagation).
target_link_libraries(cc_server PUBLIC
    cc_utils
    cc_session
    cc_query
    cc_tools
    cc_orchestration
    OpenSSL::Crypto)
