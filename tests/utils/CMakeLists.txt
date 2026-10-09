# utils/ — utility tests
#
# The former test_utils.cpp kitchen-sink (169 tests, 53 suites) is split into
# domain-specific files:
#   test_parse_references.cpp  — image/pasted/truncated-text refs, paste truncation
#   test_terminal_io.cpp       — ANSI/CSI/SGR parsing
#   test_string_utils.cpp      — string/text utilities, words, sanitization, diff
#   test_json_utils.cpp        — JSON parse/serialize, control-message key compat
#   test_token_budget.cpp      — token budget and timeout parsing
#   test_flag_settings.cpp     — --settings flag application, sources, paths, merge
#   test_misc_utils.cpp        — remaining utility suites (plugins, git, shell,
#                                clipboard, GitHub, and more) plus the shared
#                                test helpers (ScopedEnvVar, LocalGitHubApiServer, …)
#
# The test_utils target below keeps the already-separate sources that were
# previously compiled into the monolithic test_utils executable.
#
# The analytics_test.cpp source lives in services/ but is compiled as part of
# the test_utils target (it tests loom.services.analytics which is linked
# transitively through loom_utils).

# Common link libraries for every split target — the same set the monolithic
# test_utils target linked.
set(LOOM_UTILS_TEST_LIBS
    loom_core
    loom_utils           # all 50+ loom.utils.* imports
    loom_ui              # loom.ui.terminal_io
    loom_plugins         # loom.plugins.marketplace
    loom_memdir
    loom_commands        # loom.commands.review.review_remote (parse_pr_input)
    GTest::gtest_main
)

add_executable(test_parse_references test_parse_references.cpp)
target_link_libraries(test_parse_references PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_parse_references DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_terminal_io test_terminal_io.cpp)
target_link_libraries(test_terminal_io PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_terminal_io DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_string_utils test_string_utils.cpp)
target_link_libraries(test_string_utils PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_string_utils DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_json_utils test_json_utils.cpp)
target_link_libraries(test_json_utils PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_json_utils DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_token_budget test_token_budget.cpp)
target_link_libraries(test_token_budget PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_token_budget DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_flag_settings test_flag_settings.cpp)
target_link_libraries(test_flag_settings PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_flag_settings DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_misc_utils test_misc_utils.cpp)
target_link_libraries(test_misc_utils PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_misc_utils DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# Already-separate sources that were previously part of the monolithic
# test_utils executable.
add_executable(test_utils
    io_shell_memory_test.cpp
    paths_cascade_test.cpp
    permissions_test.cpp
    settings_validation_test.cpp
    ../services/analytics_test.cpp
)
target_link_libraries(test_utils PRIVATE ${LOOM_UTILS_TEST_LIBS})
gtest_discover_tests(test_utils DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
