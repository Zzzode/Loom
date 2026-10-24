# Fuzz targets for LOOM
# Only built when ENABLE_FUZZING=ON (see the "fuzz" CMake preset).

# libFuzzer supplies its own main. Coverage instrumentation is added globally
# via the fuzz preset's -fsanitize=fuzzer-no-link,address,undefined compile flags;
# each target only links the fuzzer runtime. A per-target -fsanitize=fuzzer
# COMPILE option here triggers synthetic-Consumer BMI rebuilds that clang 23
# rejects under -Werror (-Wreduced-bmi-output-overrided), so it is intentionally
# absent.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR
    "ENABLE_FUZZING requires the Homebrew Clang with libFuzzer and C++23 module support")
endif()

# Fuzz target: SSE parser
add_executable(fuzz_sse_parser fuzz_sse_parser.cpp)
target_link_options(fuzz_sse_parser PRIVATE -fsanitize=fuzzer)
target_link_libraries(fuzz_sse_parser PRIVATE loom_services loom_utils)

# Fuzz target: JSON parse + message extraction
add_executable(fuzz_json_parse fuzz_json_parse.cpp)
target_link_options(fuzz_json_parse PRIVATE -fsanitize=fuzzer)
target_link_libraries(fuzz_json_parse PRIVATE loom_utils)

# RFC 0031 E6d: Mermaid structural envelope, family parsers/layouts and the
# full dispatch. It is a module implementation unit to reach the non-exported
# parse/layout entry points, exactly like the other Mermaid structural tests.
add_executable(fuzz_mermaid_parse fuzz_mermaid_parse.cpp)
set_target_properties(fuzz_mermaid_parse PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_link_options(fuzz_mermaid_parse PRIVATE -fsanitize=fuzzer)
target_link_libraries(fuzz_mermaid_parse PRIVATE loom_ui loom_utils)
