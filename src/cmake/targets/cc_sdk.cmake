# ─── cc_sdk: embeddable SDK surface ───────────────────────────────────────────
add_library(cc_sdk)
target_sources(cc_sdk
    PUBLIC FILE_SET CXX_MODULES FILES
        sdk/control_schemas.cppm
        sdk/control_types.cppm
        sdk/core_schemas.cppm
        sdk/core_types.cppm
        sdk/runtime_types.cppm
        sdk/sandbox_types.cppm
        sdk/types.cppm
        sdk/settings_types.cppm
)
# RFC 0001 phase 2: the island gains the canonical type targets (for the
# CONVERGE aliases' BMIs) + cc_utils (for cc.serdes.json ser/de). No
# cc_query/cc_server/cc_services — the island stays free of engine/runtime
# linkage until phase 3 (§1.1 principle 3, §3.1).
target_link_libraries(cc_sdk PUBLIC
    cc_utils
    cc_types
    cc_config
    cc_tools
    cc_hooks
    cc_session
)
