# services/ — service layer tests
#
# The former 556KB monolith test_services.cpp (250 tests) has been split
# into domain-specific files below. Each target links the same set of
# libraries as the original monolith.

# Helper function: create a test target with the standard service-layer link set
function(add_loom_service_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name}
        PRIVATE
            loom_core
            loom_cli
            loom_utils
            loom_teams           # loom.teams.team_helpers
            GTest::gtest_main
    )
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
endfunction()

add_loom_service_test(test_mcp_types)
add_loom_service_test(test_mcp_client)
add_loom_service_test(test_mcp_auth)
add_loom_service_test(test_channel_permission)
add_loom_service_test(test_config)
add_loom_service_test(test_api)
add_loom_service_test(test_lsp)
add_loom_service_test(test_server)
add_loom_service_test(test_query_engine_svc)
add_loom_service_test(test_session)
add_loom_service_test(test_team_security)
add_loom_service_test(test_misc_services)


# Phase 3-E2E: SSE Mock + WithRetry unit tests
add_executable(test_sse_mock test_sse_mock.cpp)
target_link_libraries(test_sse_mock
    PRIVATE
        loom_core
        loom_services
        loom_utils
        loom_cli             # loom.cli.sse_transport (TLS SSE tests)
        httplib::httplib
        OpenSSL::SSL
        OpenSSL::Crypto
        GTest::gtest_main
)
gtest_discover_tests(test_sse_mock DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_with_retry test_with_retry.cpp)
target_link_libraries(test_with_retry
    PRIVATE
        loom_core
        loom_services            # loom.services.api.with_retry*, loom.services.oauth
        loom_utils
        GTest::gtest_main
)
gtest_discover_tests(test_with_retry DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# M7: notification backend wiring (mcp connectivity + teammate shutdowns)
add_executable(test_fix_notifs test_fix_notifs.cpp)
target_link_libraries(test_fix_notifs
    PRIVATE
        loom_core
        loom_hooks
        loom_services
        loom_orchestration   # RFC-0001 B15: loom.orchestration.mcp_connectivity + tools.mcp
        GTest::gtest_main
)
gtest_discover_tests(test_fix_notifs DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# plugin-H1-H2: marketplace fetch/add/remove + manifest validation
add_executable(test_fix_plugin test_fix_plugin.cpp)
target_link_libraries(test_fix_plugin
    PRIVATE
        loom_core
        loom_utils
        loom_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_fix_plugin DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# M4 + M11: LSP action parity + send_request<T> static_assert
add_executable(test_fix_lsp_tool test_fix_lsp_tool.cpp)
target_link_libraries(test_fix_lsp_tool
    PRIVATE
        loom_core
        GTest::gtest_main
)
gtest_discover_tests(test_fix_lsp_tool DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
