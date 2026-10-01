# ─── loom_plugins: Plugin System ────────────────────────────────────────────────
add_library(loom_plugins)
target_sources(loom_plugins
    PUBLIC FILE_SET CXX_MODULES FILES
        plugins/marketplace.cppm
        plugins/plugin.cppm
        plugins/plugin_dependency_resolver.cppm
        plugins/plugin_identifier.cppm
        plugins/plugin_lifecycle.cppm
        plugins/plugin_loader.cppm
        plugins/plugin_manager.cppm
        plugins/plugin_marketplace.cppm
        plugins/plugin_marketplace_rules.cppm
        plugins/plugin_validation.cppm
        plugins/plugin_versioning.cppm
)
# RFC 0001 Phase D B5g: plugin_identifier imports cc.config.settings_sources
# (moved to loom_config in this batch), so loom_plugins links loom_config. Acyclic:
# loom_plugins -> loom_config -> loom_utils.
target_link_libraries(loom_plugins PUBLIC loom_utils loom_types loom_config yyjson uv_a)
