# utils/ — utility tests
#
# test_utils.cpp is a 128KB kitchen-sink file (169 tests, 53 suites) covering
# JSON, string utils, terminal I/O, plugin loading, git diff, and more.
# It is being split into domain-specific files. Until the split is complete,
# the original file remains here alongside the already-separated targets.
#
# The analytics_test.cpp source lives in services/ but is compiled as part of
# the test_utils target (it tests loom.services.analytics which is linked
# transitively through loom_utils).

add_executable(test_utils
    test_utils.cpp
    io_shell_memory_test.cpp
    paths_cascade_test.cpp
    permissions_test.cpp
    settings_validation_test.cpp
    ../services/analytics_test.cpp
)
target_link_libraries(test_utils
    PRIVATE
        loom_core
        loom_utils           # all 50+ loom.utils.* imports
        loom_ui              # loom.ui.terminal_io
        loom_plugins         # loom.plugins.marketplace
        loom_memdir
        loom_commands        # loom.commands.review.review_remote (parse_pr_input)
        GTest::gtest_main
)
gtest_discover_tests(test_utils DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
