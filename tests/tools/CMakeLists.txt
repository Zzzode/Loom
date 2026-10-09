# tools/ — tool implementation tests
#
# test_tools.cpp is a 581KB monolith (273 tests) covering 20+ tool domains.
# It is being split into domain-specific files. Until the split is complete,
# the original file remains here alongside the already-separated targets.

# RFC-0001 B15: the second TU performs the pre-main install_runtime_backends
# slot assignment (module TU; scanned by clang-scan-deps like every source).
add_executable(test_tools test_tools.cpp test_runtime_backends_install.cpp)
target_link_libraries(test_tools
    PRIVATE
        loom_core
        loom_tools           # all 40+ loom.tools.* imports
        loom_orchestration   # RFC-0001 B11: loom.orchestration.runtime_backends
        loom_hooks           # loom.hooks.tool_permissions
        loom_services        # loom.services.api.client, loom.services.mcp.*
        loom_utils           # loom.utils.* helpers
        GTest::gtest_main
)
gtest_discover_tests(test_tools DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})


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
