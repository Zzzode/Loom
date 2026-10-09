# commands/ — slash command tests

add_executable(test_commands test_commands.cpp)
target_link_libraries(test_commands
    PRIVATE
        loom_core
        loom_commands            # loom.commands.*
        loom_query               # loom.query.query_engine
        loom_types               # loom.types.types
        GTest::gtest_main
)
gtest_discover_tests(test_commands DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


# Statusline (commands + utils/statusline_runner) unit tests
add_executable(test_statusline test_statusline.cpp)
target_link_libraries(test_statusline
    PRIVATE
        loom_core
        loom_commands            # loom.commands.statusline
        loom_utils               # loom.serdes.json (statusline_runner now in loom_ui_app)
        GTest::gtest_main
)
gtest_discover_tests(test_statusline DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
