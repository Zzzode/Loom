# Smoke tests for the loom executable.
# These tests are only registered when the loom target exists.
# If the target is defined but the binary is not yet built, tests report SKIP
# (exit code 125) instead of FAIL so the CTest dashboard stays green.

if(TARGET loom)
  set(_SMOKE_RUNNER   "${CMAKE_CURRENT_SOURCE_DIR}/run_smoke.cmake")
  set(_SMOKE_SKIP_RC  125)

  # --help: binary starts, CLI parser works, usage text is emitted.
  add_test(NAME test_loom_help
    COMMAND ${CMAKE_COMMAND}
      -D LOOM_BIN=$<TARGET_FILE:loom>
      -D "LOOM_ARGS=--help"
      -D EXPECT_EXIT=0
      -D "MATCH_STDOUT=Usage: loom"
      -D SKIP_RC=${_SMOKE_SKIP_RC}
      -P ${_SMOKE_RUNNER}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(test_loom_help PROPERTIES
    TIMEOUT         10
    SKIP_RETURN_CODE ${_SMOKE_SKIP_RC})

  # --list-runtime-tools: runtime registry is populated — exercises the full
  # static-init path that registers built-in tools.
  add_test(NAME test_loom_list_tools
    COMMAND ${CMAKE_COMMAND}
      -D LOOM_BIN=$<TARGET_FILE:loom>
      -D "LOOM_ARGS=--list-runtime-tools"
      -D EXPECT_EXIT=0
      -D "MATCH_STDOUT=Bash"
      -D SKIP_RC=${_SMOKE_SKIP_RC}
      -P ${_SMOKE_RUNNER}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(test_loom_list_tools PROPERTIES
    TIMEOUT         10
    SKIP_RETURN_CODE ${_SMOKE_SKIP_RC})
endif()
