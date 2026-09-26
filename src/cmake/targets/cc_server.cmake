# ─── cc_server: Server ────────────────────────────────────────────────────────
add_library(cc_server)
target_sources(cc_server
    PUBLIC FILE_SET CXX_MODULES FILES
        server/server_main.cppm
        server/server_routes.cppm
        server/types.cppm
)
# RFC-0001 B11: cc_orchestration is PUBLIC — server_main.cppm and the
# test_services TU import cc.server.server_routes, whose module interface
# imports cc.orchestration.runtime_backends; consumers need its BMI on their
# compile line.
target_link_libraries(cc_server PUBLIC
    cc_utils
    cc_session
    cc_query
    cc_tools
    cc_orchestration
    OpenSSL::Crypto)
