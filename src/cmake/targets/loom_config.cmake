# ─── loom_config: Configuration ─────────────────────────────────────────────────
add_library(loom_config)
target_sources(loom_config
    PUBLIC FILE_SET CXX_MODULES FILES
        config/mcp_types.cppm
        config/config.cppm
        config/feature_flags.cppm
        config/settings.cppm
        # RFC 0001 Phase D B5g: moved from loom_utils (src/utils/settings/).
        # Import loom.serdes.json (loom_utils, linked PUBLIC below) and each
        # other; MODULE_RANK_OVERRIDE ranks them 2 (D5) so the serdes edge
        # is not an upward edge (loom.config is rank 1).
        config/settings_manager.cppm
        config/settings_merge.cppm
        config/settings_paths.cppm
        config/settings_sources.cppm
        config/settings_validation.cppm
)
target_link_libraries(loom_config PUBLIC loom_utils loom_types loom_constants)
