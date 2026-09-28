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
