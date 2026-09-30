# ─── cc_plugins: Plugin System ────────────────────────────────────────────────
add_library(cc_plugins)
target_sources(cc_plugins
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
# (moved to cc_config in this batch), so cc_plugins links cc_config. Acyclic:
# cc_plugins -> cc_config -> cc_utils.
target_link_libraries(cc_plugins PUBLIC cc_utils cc_types cc_config yyjson uv_a)
