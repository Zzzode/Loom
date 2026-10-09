# tools/ — tool implementation tests
#
# The former 581KB monolith test_tools.cpp (273 tests) has been split into
# domain-specific files. Each target links the same set of libraries; the
# split is purely organisational — every TU still imports the full module
# set, so unused-import warnings do not apply (C++ modules have no such
# warning).

# RFC-0001 B15: the second TU performs the pre-main install_runtime_backends
# slot assignment (module TU; scanned by clang-scan-deps like every source).
# The original monolith linked it once for the whole binary; after the split
# every target whose tests resolve through loom.tools.runtime_backends.port
# (Agent, config, lsp, mcp*, computer_use dispatch) needs it. Targets whose
# tests all go through FileToolServicesGuard (which call_once-installs the
# same slots) do not.

set(TOOL_TEST_LIBS
    loom_core
    loom_tools
    loom_orchestration
    loom_hooks
    loom_services
    loom_utils
    GTest::gtest_main
)

add_executable(test_agent test_agent.cpp test_runtime_backends_install.cpp)
target_link_libraries(test_agent PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_agent DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_bash test_bash.cpp)
target_link_libraries(test_bash PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_bash DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_computer_use test_computer_use.cpp test_runtime_backends_install.cpp)
target_link_libraries(test_computer_use PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_computer_use DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_file_ops test_file_ops.cpp)
target_link_libraries(test_file_ops PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_file_ops DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_permission_grants test_permission_grants.cpp)
target_link_libraries(test_permission_grants PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_permission_grants DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_powershell test_powershell.cpp)
target_link_libraries(test_powershell PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_powershell DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_runtime_config test_runtime_config.cpp test_runtime_backends_install.cpp)
target_link_libraries(test_runtime_config PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_runtime_config DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_swarm test_swarm.cpp)
target_link_libraries(test_swarm PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_swarm DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_tools_misc test_tools_misc.cpp test_runtime_backends_install.cpp)
target_link_libraries(test_tools_misc PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_tools_misc DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_web test_web.cpp)
target_link_libraries(test_web PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_web DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_wire_seam test_wire_seam.cpp)
target_link_libraries(test_wire_seam PRIVATE ${TOOL_TEST_LIBS})
gtest_discover_tests(test_wire_seam DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_tasks test_tasks.cpp)
target_link_libraries(test_tasks
    PRIVATE
        loom_core
        loom_tasks               # loom.tasks.*
        loom_tools               # loom.tools.agent_* / team / spawn_multi_agent
        loom_utils               # loom.utils.* helpers (loom.teams.swarm.* arrives via loom_tools)
        GTest::gtest_main
)
gtest_discover_tests(test_tasks DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


add_executable(test_skills test_skills.cpp)
target_link_libraries(test_skills
    PRIVATE
        loom_core
        loom_skills              # loom.skills.skill, loom.skills.bundled
        loom_skills_core
        GTest::gtest_main
)
gtest_discover_tests(test_skills DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
