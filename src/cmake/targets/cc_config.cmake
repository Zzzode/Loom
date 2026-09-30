# ─── cc_config: Configuration ─────────────────────────────────────────────────
add_library(cc_config)
target_sources(cc_config
    PUBLIC FILE_SET CXX_MODULES FILES
        config/mcp_types.cppm
        config/config.cppm
        config/feature_flags.cppm
        config/settings.cppm
        # RFC 0001 Phase D B5g: moved from cc_utils (src/utils/settings/).
        # Import cc.serdes.json (cc_utils, linked PUBLIC below) and each
        # other; MODULE_RANK_OVERRIDE ranks them 2 (D5) so the serdes edge
        # is not an upward edge (cc.config is rank 1).
        config/settings_manager.cppm
        config/settings_merge.cppm
        config/settings_paths.cppm
        config/settings_sources.cppm
        config/settings_validation.cppm
)
target_link_libraries(cc_config PUBLIC cc_utils cc_types cc_constants)
