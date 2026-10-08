# ─── loom_sdk: embeddable SDK surface ───────────────────────────────────────────
add_library(loom_sdk)
target_sources(loom_sdk
    PUBLIC FILE_SET CXX_MODULES FILES
        sdk/core_schemas.cppm
        sdk/core_types.cppm
        sdk/runtime_types.cppm
        sdk/sandbox_types.cppm
        # RFC 0001 cc-sdk phase 3 (§2.2): the opaque harness embedding
        # entrypoint. Bodies in harness.cpp (impl unit), not the BMI.
        sdk/harness.cppm
)
# RFC 0001 cc-sdk phase 3 (§2.2): the harness wraps loom.query.assembly (the
# extracted engine recipe), so loom_sdk links loom_query — the first engine
# linkage in the SDK closure. This is downward (16 -> 10) and phase 3 (the
# harness). loom_query PUBLICly links loom_tools/loom_hooks/loom_session/loom_memdir/
# loom_services/loom_orchestration (loom_tools.cmake + loom_query.cmake), so the
# assembly's deps are already in the closure.
#
# Link-closure cost (accepted, §3.4): loom_query PUBLIC-links loom_services
# (loom_tools.cmake:133) and CURL::libcurl (loom_query.cmake:42); loom_services
# PUBLIC-links OpenSSL::SSL/Crypto, CURL::libcurl, httplib, yyjson, uv_a
# (loom_services.cmake:66-81). So OpenSSL and libcurl DO enter the SDK
# closure via loom_services — the phase-3 cost of embedding the engine.
# (loom_services' module interfaces expose OpenSSL/CURL types — e.g.
# gcp_adc.cppm's EvpPkeyPtr and client.cppm's CURL handle — so those links
# cannot be made PRIVATE to stop propagation.) What stays out is the
# loom_server target itself (the HTTP server routes/main): loom_sdk does NOT
# link loom_server. (httplib was already in the closure via loom_utils —
# pre-existing.) The phase-2 canonical type targets
# (loom_utils/loom_types/loom_config/loom_tools/loom_hooks/loom_session) remain for the
# CONVERGE aliases' BMIs.
target_sources(loom_sdk
    PRIVATE
        sdk/harness.cpp
)
target_link_libraries(loom_sdk PUBLIC
    loom_utils
    loom_types
    loom_config
    loom_tools
    loom_hooks
    loom_session
    loom_query
)
