# ─── loom_benchmarks: Pare Benchmark System ─────────────────────────────────────
add_library(loom_benchmarks)
target_sources(loom_benchmarks
    PUBLIC FILE_SET CXX_MODULES FILES
        benchmarks/pare/case_loader.cppm
        benchmarks/pare/cli.cppm
        benchmarks/pare/evaluator.cppm
        benchmarks/pare/execute_ref.cppm
        benchmarks/pare/metrics.cppm
        benchmarks/pare/run.cppm
        benchmarks/pare/schema.cppm
        benchmarks/pare/workspace.cppm
)
target_link_libraries(loom_benchmarks PUBLIC loom_utils yyjson)
