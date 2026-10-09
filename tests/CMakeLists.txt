# LOOM Test Suite
#
# Structure:
#   loom_test_main.cpp  — shared test main (env scrubbing, history redirect)
#   support/            — shared test headers (test_ui_helpers.h, etc.)
#   core/               — engine, state, bootstrap, config tests
#   tools/              — tool implementation tests (test_tools.cpp split)
#   services/           — service layer tests (test_services.cpp split)
#   ui/                 — FTXUI interface tests (dialogs, streaming, markdown)
#   bridge/             — bridge daemon + SDK protocol tests
#   commands/           — slash command tests
#   utils/              — utility tests (test_utils.cpp split)
#   query/              — query engine, hooks, wire backend tests
#   fixtures/ golden/   — test data (not CMake targets)
#   smoke/ e2e/ fuzz/ benchmarks/ — specialized test suites

include(FetchContent)


FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Disable C++ module scanning for GoogleTest (it doesn't use modules)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(_LOOM_TEST_WERROR_OLD ${CMAKE_COMPILE_WARNING_AS_ERROR})
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
FetchContent_MakeAvailable(googletest)
set(CMAKE_COMPILE_WARNING_AS_ERROR ${_LOOM_TEST_WERROR_OLD})
unset(_LOOM_TEST_WERROR_OLD)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)


include(GoogleTest)


# NOTE: The test TUs below import C++20 named modules (`import loom.xxx.yyy;`)
# extensively, so CMAKE_CXX_SCAN_FOR_MODULES MUST remain ON at directory scope.
# Only the googletest vendor targets (which do not export or import modules)
# have scanning explicitly disabled via set_target_properties below.  Module
# scanning was previously disabled at directory scope under the incorrect
# assumption that tests did not import modules — that caused every test TU to
# fail with "fatal error: module 'loom.*' not found" because BMI discovery was
# skipped even though the libraries were built correctly.
set(LOOM_TEST_DISCOVERY_TIMEOUT 30 CACHE STRING "Timeout in seconds for GoogleTest discovery")

# Turn scanning OFF for the googletest vendor targets — they contain no module
# interfaces or imports and would otherwise waste scanner time / confuse BMI
# dependency tracking on some CMake/Ninja combos.
foreach(_gt_target IN ITEMS gtest gtest_main gmock gmock_main)
    if(TARGET ${_gt_target})
        set_target_properties(${_gt_target} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    endif()
endforeach()

# Custom test main: scrubs terminal-identifying env vars (TERM_PROGRAM,
# LC_TERMINAL, WT_SESSION, VTE_VERSION) so golden snapshots are deterministic
# across developer machines — OSC 8 hyperlink emission is gated on these.
# Golden binaries link this instead of GTest::gtest_main.
add_library(loom_test_main STATIC loom_test_main.cpp)
target_link_libraries(loom_test_main PUBLIC GTest::gtest ftxui::screen)
set_target_properties(loom_test_main PROPERTIES CXX_SCAN_FOR_MODULES OFF)

# Shared test support headers (test_ui_helpers.h, invariant_checker.hpp, etc.)
set(LOOM_TEST_SUPPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/support)
include_directories(${LOOM_TEST_SUPPORT_DIR})

# Absolute path to the tests/ source directory, for tests that locate
# golden/ and fixtures/ via __FILE__-relative paths. After the domain
# restructuring, test files live in subdirectories (ui/, bridge/, ...),
# so __FILE__-relative paths no longer resolve to the shared golden/
# and fixtures/ directories.
add_compile_definitions(LOOM_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Domain test suites (each subdirectory has its own CMakeLists.txt)
# ---------------------------------------------------------------------------
add_subdirectory(core)
add_subdirectory(tools)
add_subdirectory(services)
add_subdirectory(ui)
add_subdirectory(bridge)
add_subdirectory(commands)
add_subdirectory(utils)
add_subdirectory(query)

# ---------------------------------------------------------------------------
# Phase 3-B/C: tools_smoke + tools_e2e
#
# These two executables exercise the real bash fork/exec and filesystem
# primitives in loom.tools.bash.impl / loom.tools.files.impl with real shell
# and filesystem side-effects.  Instead of linking through `loom_tools`
# (which transitively pulls in loom_services and can be broken by unrelated
# module scan issues like services/api/sse_client.ddi) we build them via
# the standalone helper script scripts/build_tools_standalone.bash which
# precompiles only the two impl_* .cppm units and links against the
# system C++ runtime.  We still register them as CTest tests so the
# standard `ctest -R tools_*` / `ctest --output-on-failure` invocation
# works consistently.
#
# Both binaries land in ${CMAKE_BINARY_DIR}:
#   ${CMAKE_BINARY_DIR}/tools_smoke  — fine-grained CHECK assertions
#   ${CMAKE_BINARY_DIR}/tools_e2e    — Bash × File cross-tool scenario
# ---------------------------------------------------------------------------
set(LOOM_TOOLS_STANDALONE_BUILDER
    "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build_tools_standalone.bash")

add_custom_command(
    OUTPUT
        "${CMAKE_BINARY_DIR}/tools_smoke"
        "${CMAKE_BINARY_DIR}/tools_e2e"
    COMMAND
        ${CMAKE_COMMAND} -E env
            "LOOM_TOOLS_STANDALONE_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "CXX=${CMAKE_CXX_COMPILER}"
            bash "${LOOM_TOOLS_STANDALONE_BUILDER}" all
    DEPENDS
        "${LOOM_TOOLS_STANDALONE_BUILDER}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/tools_smoke.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/tools_e2e.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/bash/impl_bash.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/files/impl_files.cppm"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    COMMENT "[Phase 3-B/C] Building standalone tools_smoke + tools_e2e"
    VERBATIM
)
add_custom_target(tools_standalone DEPENDS
    "${CMAKE_BINARY_DIR}/tools_smoke"
    "${CMAKE_BINARY_DIR}/tools_e2e")

# A "setup" CTest test that drives the custom-target build so that
# `ctest -R tools` works without an explicit `cmake --build ... --target
# tools_standalone` step.  We also declare it a FIXTURES_SETUP so the two
# actual assertions only run after the binaries are up to date.
add_test(
    NAME tools_standalone_setup
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target tools_standalone
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
set_tests_properties(tools_standalone_setup PROPERTIES
    TIMEOUT 300
    LABELS "phase3;tools;setup"
    FIXTURES_SETUP tools_standalone_fixture
)

add_test(
    NAME tools_smoke
    COMMAND "${CMAKE_BINARY_DIR}/tools_smoke"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
set_tests_properties(tools_smoke PROPERTIES
    TIMEOUT 60
    LABELS "phase3;tools;smoke"
    FIXTURES_REQUIRED tools_standalone_fixture
)

add_test(
    NAME tools_e2e
    COMMAND "${CMAKE_BINARY_DIR}/tools_e2e"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
set_tests_properties(tools_e2e PROPERTIES
    TIMEOUT 60
    LABELS "phase3;tools;e2e"
    FIXTURES_REQUIRED tools_standalone_fixture
)

# ---------------------------------------------------------------------------
# Phase 3-G PermissionResolver + PermissionGate standalone smoke runner.
# Exercises all 5 acceptance cases + glob + ABI + gate wiring.
# ---------------------------------------------------------------------------
add_executable(phase3_permission_smoke
    smoke/phase3_permission_smoke.cpp
)
target_link_libraries(phase3_permission_smoke
    PRIVATE
        loom_core
        loom_hooks
        loom_utils
)
set_target_properties(phase3_permission_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME Phase3_Permission_Smoke COMMAND phase3_permission_smoke)
set_tests_properties(Phase3_Permission_Smoke PROPERTIES TIMEOUT 10 LABELS "phase3;permission;hooks")

# ---------------------------------------------------------------------------
# Smoke tests for the loom executable itself.  Kept in its own subdirectory:
# it uses a cmake -P script runner with SKIP_RETURN_CODE rather than
# GoogleTest discovery.
# ---------------------------------------------------------------------------
add_subdirectory(smoke)

# Phase 3-E2E: end-to-end integration tests (3 binaries, plain .cpp main,
# no GoogleTest dep — keep them lightweight).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/e2e/CMakeLists.txt)
  add_subdirectory(e2e)
endif()

# ---------------------------------------------------------------------------
# Fuzz targets (only when ENABLE_FUZZING is ON)
# ---------------------------------------------------------------------------
if(ENABLE_FUZZING)
  add_subdirectory(fuzz)
endif()

# Google Benchmark micro-benchmark suite
add_subdirectory(benchmarks)
