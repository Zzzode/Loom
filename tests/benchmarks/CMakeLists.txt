# ─── Google Benchmark micro-benchmark suite ──────────────────────────────────

add_executable(bench_core bench_core.cpp)

# bench_core.cpp imports C++20 named modules (e.g. cc.services.api.sse) so it
# MUST be scanned; otherwise CMake never wires upstream BMI dependencies.
set_target_properties(bench_core PROPERTIES
    CXX_SCAN_FOR_MODULES ON
)
target_compile_options(bench_core PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c2y-extensions>
)

target_link_libraries(bench_core
    PRIVATE
        loom_core              # full module BMI propagation (defense-in-depth)
        loom_services          # cc.services.api.sse
        loom_tools
        loom_utils
        benchmark::benchmark
        benchmark::benchmark_main
)

# Native regressions for Pare execution, aggregation, and CLI reporting.
add_executable(test_pare test_pare.cpp)
target_link_libraries(test_pare PRIVATE loom_benchmarks GTest::gtest_main)
gtest_discover_tests(test_pare DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})
