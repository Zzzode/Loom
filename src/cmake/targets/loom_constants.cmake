# ─── loom_constants: Constants ──────────────────────────────────────────────────
add_library(loom_constants)
target_sources(loom_constants
    PUBLIC FILE_SET CXX_MODULES FILES
        constants/constants.cppm
        constants/cost_tracker.cppm
        constants/paths.cppm
        constants/product.cppm
        constants/prompts.cppm
        constants/spinner_verbs.cppm
        services/api/sse_client.cppm
        constants/xml.cppm
)
target_link_libraries(loom_constants PUBLIC loom_utils loom_types)
