# ─── cc_sdk: embeddable SDK surface ───────────────────────────────────────────
add_library(cc_sdk)
target_sources(cc_sdk
    PUBLIC FILE_SET CXX_MODULES FILES
        sdk/core_schemas.cppm
        sdk/core_types.cppm
        sdk/runtime_types.cppm
        sdk/sandbox_types.cppm
        # RFC 0001 cc-sdk phase 3 (§2.2): the opaque harness embedding
        # entrypoint. Bodies in harness.cpp (impl unit), not the BMI.
        sdk/harness.cppm
)
# RFC 0001 cc-sdk phase 3 (§2.2): the harness wraps cc.query.assembly (the
# extracted engine recipe), so cc_sdk links cc_query — the first engine
# linkage in the SDK closure. This is downward (16 -> 10) and phase 3 (the
# harness). cc_query PUBLICly links cc_tools/cc_hooks/cc_session/cc_memdir/
# cc_services/cc_orchestration (cc_tools.cmake + cc_query.cmake), so the
# assembly's deps are already in the closure.
#
# Link-closure cost (accepted, §3.4): cc_query PUBLIC-links cc_services
# (cc_tools.cmake:133) and CURL::libcurl (cc_query.cmake:42); cc_services
# PUBLIC-links OpenSSL::SSL/Crypto, CURL::libcurl, httplib, yyjson, uv_a
# (cc_services.cmake:66-81). So OpenSSL and libcurl DO enter the SDK
# closure via cc_services — the phase-3 cost of embedding the engine.
# (cc_services' module interfaces expose OpenSSL/CURL types — e.g.
# gcp_adc.cppm's EvpPkeyPtr and client.cppm's CURL handle — so those links
# cannot be made PRIVATE to stop propagation.) What stays out is the
# cc_server target itself (the HTTP server routes/main): cc_sdk does NOT
# link cc_server. (httplib was already in the closure via cc_utils —
# pre-existing.) The phase-2 canonical type targets
# (cc_utils/cc_types/cc_config/cc_tools/cc_hooks/cc_session) remain for the
# CONVERGE aliases' BMIs.
target_sources(cc_sdk
    PRIVATE
        sdk/harness.cpp
)
target_link_libraries(cc_sdk PUBLIC
    cc_utils
    cc_types
    cc_config
    cc_tools
    cc_hooks
    cc_session
    cc_query
)
