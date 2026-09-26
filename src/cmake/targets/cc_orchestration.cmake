# ─── cc_orchestration: rank-9 runtime backend composition ────────────────────
# RFC 0001 Phase B. This target owns the modules that wire concrete
# lower-ranked services into the tools-layer callback ports and host the
# lifted runtime tool backends. Born in B11 with one module
# (cc.orchestration.runtime_backends, image-codec installer only); later
# batches add the skill executor and the lifted agent/mcp/lsp backends.
add_library(cc_orchestration)
target_sources(cc_orchestration
    PUBLIC FILE_SET CXX_MODULES FILES
        orchestration/runtime_backends.cppm
)
# Direct links correspond to this batch's direct module imports only:
# cc.services.image (cc_services) and cc.tools.image_codec.port (cc_tools).
# cc_types/cc_utils/cc_skills_core reach this target through the PUBLIC
# links of those two.
target_link_libraries(cc_orchestration
    PUBLIC
        cc_tools
        cc_services
)
