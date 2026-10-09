# query/ — query engine, hooks, and wire backend tests

add_executable(test_hooks test_hooks.cpp)
target_link_libraries(test_hooks
    PRIVATE
        loom_core
        loom_hooks           # all loom.hooks.* imports
        loom_utils           # loom.serdes.json
        GTest::gtest_main
)
gtest_discover_tests(test_hooks DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_wire_backends test_wire_backends.cpp)
target_link_libraries(test_wire_backends
    PRIVATE
        loom_core
        loom_query           # loom.query.wire_protocol + backend implementations
        loom_utils
        loom_types
        loom_tools
        GTest::gtest_main
)
gtest_discover_tests(test_wire_backends DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# B1-qe-tests: QueryEngine coverage (output_config, fallback models, memory, skills)
add_executable(test_fix_query_engine test_fix_query_engine.cpp)
target_link_libraries(test_fix_query_engine
    PRIVATE
        loom_core
        GTest::gtest_main
)
gtest_discover_tests(test_fix_query_engine DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# B2: loom.hooks.execution wired into QueryEngine tool-call dispatch
add_executable(test_fix_hooks_integration test_fix_hooks_integration.cpp)
target_link_libraries(test_fix_hooks_integration
    PRIVATE
        loom_core
        GTest::gtest_main
)
gtest_discover_tests(test_fix_hooks_integration DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
