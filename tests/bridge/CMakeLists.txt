# bridge/ — bridge daemon + SDK protocol tests
#
# test_bridge.cpp was split into three domain-specific TUs sharing
# tests/support/bridge_test_helpers.hpp (on the include path via
# LOOM_TEST_SUPPORT_DIR in tests/CMakeLists.txt).

set(_bridge_test_libs
    loom_core
    loom_daemon
    GTest::gtest_main
)

add_executable(test_bridge_transport
    test_bridge_transport.cpp
    ${LOOM_TEST_SUPPORT_DIR}/bridge_test_helpers.hpp)
target_link_libraries(test_bridge_transport PRIVATE ${_bridge_test_libs})
add_dependencies(test_bridge_transport loom)
target_compile_definitions(test_bridge_transport
    PRIVATE
        LOOM_NATIVE_BINARY_PATH="$<TARGET_FILE:loom>"
)
gtest_discover_tests(test_bridge_transport DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_bridge_api
    test_bridge_api.cpp
    ${LOOM_TEST_SUPPORT_DIR}/bridge_test_helpers.hpp)
target_link_libraries(test_bridge_api PRIVATE ${_bridge_test_libs})
add_dependencies(test_bridge_api loom)
target_compile_definitions(test_bridge_api
    PRIVATE
        LOOM_NATIVE_BINARY_PATH="$<TARGET_FILE:loom>"
)
gtest_discover_tests(test_bridge_api DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_bridge_daemon
    test_bridge_daemon.cpp
    ${LOOM_TEST_SUPPORT_DIR}/bridge_test_helpers.hpp)
target_link_libraries(test_bridge_daemon PRIVATE ${_bridge_test_libs})
add_dependencies(test_bridge_daemon loom)
target_compile_definitions(test_bridge_daemon
    PRIVATE
        LOOM_NATIVE_BINARY_PATH="$<TARGET_FILE:loom>"
)
gtest_discover_tests(test_bridge_daemon DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# ---------------------------------------------------------------------------
# loom_sdk phases 2-3 (RFC 0001): golden wire-compatibility gate for the
# hand-rolled control protocol spoken by loom.server.server_main and
# loom.bridge.bridge_messaging.
# ---------------------------------------------------------------------------
add_executable(test_sdk_control_golden test_sdk_control_golden.cpp)
target_link_libraries(test_sdk_control_golden
    PRIVATE
        loom_core            # loom.server.server_main + loom.bridge.bridge_messaging BMIs
        loom_utils           # loom.serdes.json
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_sdk_control_golden
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# loom_sdk phases 2-3 (RFC 0001): round-trip ser/de tests for every moved
# control DTO (design §4.2). Links loom_sdk + loom_server per the design.
# ---------------------------------------------------------------------------
add_executable(test_sdk_serde test_sdk_serde.cpp)
target_link_libraries(test_sdk_serde
    PRIVATE
        loom_sdk
        loom_server          # loom.server.control_protocol BMIs
        GTest::gtest_main
)
gtest_discover_tests(test_sdk_serde
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# loom_sdk phases 2-3 (RFC 0001): compile-time guards that the SDK island's
# CONVERGE aliases point at the canonical types (design §4.2).
# ---------------------------------------------------------------------------
add_executable(test_sdk_convergence test_sdk_convergence.cpp)
target_link_libraries(test_sdk_convergence
    PRIVATE
        loom_sdk
        GTest::gtest_main
)
gtest_discover_tests(test_sdk_convergence
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# loom_sdk phases 2-3 (RFC 0001): first-consumer gate for loom.sdk.harness
# (design §4.4). Runs a Harness against a loopback HTTP server serving
# canned Anthropic responses.
# ---------------------------------------------------------------------------
add_executable(test_sdk_harness test_sdk_harness.cpp)
target_link_libraries(test_sdk_harness
    PRIVATE
        loom_sdk
        loom_query
        httplib::httplib
        GTest::gtest_main
)
gtest_discover_tests(test_sdk_harness
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
