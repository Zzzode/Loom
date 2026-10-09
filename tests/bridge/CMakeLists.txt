# bridge/ — bridge daemon + SDK protocol tests

add_executable(test_bridge test_bridge.cpp)
target_link_libraries(test_bridge
    PRIVATE
        loom_core
        loom_daemon
        GTest::gtest_main
)
add_dependencies(test_bridge loom)
target_compile_definitions(test_bridge
    PRIVATE
        LOOM_NATIVE_BINARY_PATH="$<TARGET_FILE:loom>"
)
gtest_discover_tests(test_bridge DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


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
