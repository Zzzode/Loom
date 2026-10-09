# commands/ — slash command tests

# CommandRegistry + AppCommandRegistry (dispatch, config, mcp, xaa)
add_executable(test_command_registry test_command_registry.cpp)
target_link_libraries(test_command_registry
    PRIVATE
        loom_core
        loom_commands            # loom.commands.*
        loom_query               # loom.query.query_engine
        loom_types               # loom.types.types
        GTest::gtest_main
)
gtest_discover_tests(test_command_registry DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# Insights facet pipeline
add_executable(test_insights test_insights.cpp)
target_link_libraries(test_insights
    PRIVATE
        loom_core
        loom_commands            # loom.commands.*
        loom_query               # loom.query.query_engine
        loom_types               # loom.types.types
        GTest::gtest_main
)
gtest_discover_tests(test_insights DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# Plugin command
add_executable(test_plugin_command test_plugin_command.cpp)
target_link_libraries(test_plugin_command
    PRIVATE
        loom_core
        loom_commands            # loom.commands.*
        loom_query               # loom.query.query_engine
        loom_types               # loom.types.types
        GTest::gtest_main
)
gtest_discover_tests(test_plugin_command DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# TerminalSetupCommand + ResumeCommand
add_executable(test_misc_commands test_misc_commands.cpp)
target_link_libraries(test_misc_commands
    PRIVATE
        loom_core
        loom_commands            # loom.commands.*
        loom_query               # loom.query.query_engine
        loom_types               # loom.types.types
        GTest::gtest_main
)
gtest_discover_tests(test_misc_commands DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


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
