# core/ — engine, state, bootstrap, config, and misc regression tests

add_executable(test_core test_core.cpp)
target_link_libraries(test_core
    PRIVATE
        loom_core
        loom_types           # import loom.types.types
        loom_config          # import loom.config.*
        loom_constants       # import loom.constants.constants
        loom_coordinator     # import loom.coordinator.types
        loom_tasks           # import loom.tasks.task_graph
        GTest::gtest_main
)
gtest_discover_tests(test_core DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_bootstrap test_bootstrap.cpp)
target_link_libraries(test_bootstrap
    PRIVATE
        loom_core
        loom_bootstrap           # loom.bootstrap.interactive
        loom_history             # loom.history
        loom_task_types          # loom.task_types
        loom_state               # loom.state.teammate_view_helpers
        loom_utils               # loom.serdes.json
        GTest::gtest_main
)
gtest_discover_tests(test_bootstrap DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# RFC 0002 F2 composition-root tests — doctor renderer through the registry
# (screens-side registration) and wizard/trust flows through the
# feature_dialog_protocol leaf (composition-root factory registration).
add_executable(test_f2_composition_root test_f2_composition_root.cpp)
target_link_libraries(test_f2_composition_root
    PRIVATE
        loom_core
        loom_ui                  # loom.ui.dialogs.* + loom.ui.screens.* + loom.ui.app.*
        GTest::gtest_main
)
gtest_discover_tests(test_f2_composition_root
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_state test_state.cpp)
target_link_libraries(test_state
    PRIVATE
        loom_core
        loom_state           # all loom.state.* imports
        loom_session         # loom.session.history
        loom_types           # loom.types.types
        loom_cli             # loom.cli.update
        loom_constants       # loom.constants.prompts
        loom_services        # loom.services.mcp.*
        loom_hooks           # loom.hooks.tool_permissions
        loom_ui              # loom.ui.repl_screen / loom.ui.* (broad coverage)
        GTest::gtest_main
)
gtest_discover_tests(test_state DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# state-layer: M2 fsync atomic write + M1 undo/redo regression
add_executable(test_fix_state test_fix_state.cpp)
target_link_libraries(test_fix_state
    PRIVATE
        loom_core
        loom_state
        loom_utils
        GTest::gtest_main
)
gtest_discover_tests(test_fix_state DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# H5: yoga layout engine subset regression (documentation-only fix, 18 tests)
add_executable(test_fix_yoga test_fix_yoga.cpp)
target_link_libraries(test_fix_yoga
    PRIVATE
        loom_core
        GTest::gtest_main
)
gtest_discover_tests(test_fix_yoga DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# misc-low: oauth_port bind-probe, secret_scanner gitleaks, in_process_transport, PKCE CSPRNG
add_executable(test_fix_misc_low test_fix_misc_low.cpp)
target_link_libraries(test_fix_misc_low
    PRIVATE
        loom_core
        loom_services
        GTest::gtest_main
)
gtest_discover_tests(test_fix_misc_low DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# P0-5 / P0-6: Enterprise Auth (SigV4 / GCP ADC / Azure DefaultAzureCredential
# / Provider selector).  Link OpenSSL::Crypto for the GCP service-account
# RS256 self-signed JWT signer.
add_executable(test_enterprise_auth test_enterprise_auth.cpp)
target_link_libraries(test_enterprise_auth
    PRIVATE
        loom_core
        loom_services
        loom_utils
        httplib::httplib
        OpenSSL::Crypto
        GTest::gtest_main
)
gtest_discover_tests(test_enterprise_auth DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# Phase 3-D: MCP stdio transport + mock echo server
add_executable(test_mcp_stdio test_mcp_stdio.cpp)
target_link_libraries(test_mcp_stdio
    PRIVATE
        loom_core
        loom_services
        loom_utils
        GTest::gtest_main
)
gtest_discover_tests(test_mcp_stdio DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
