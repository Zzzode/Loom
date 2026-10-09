# services/ — service layer tests
#
# test_services.cpp is a 556KB monolith (250 tests) covering MCP, config,
# API client, LSP, server, XAA auth, and more. It is being split into
# domain-specific files. Until the split is complete, the original file
# remains here alongside the already-separated targets.

add_executable(test_services test_services.cpp)
target_link_libraries(test_services
    PRIVATE
        loom_core
        loom_cli
        loom_utils
        loom_teams           # loom.teams.team_helpers
        GTest::gtest_main
)
gtest_discover_tests(test_services DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


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
