# ─── cc_orchestration: rank-9 runtime backend composition ────────────────────
# RFC 0001 Phase B. This target owns the modules that wire concrete
# lower-ranked services into the tools-layer callback ports and host the
# lifted runtime tool backends (agent subtree, MCP, LSP, computer-use, the
# MCP-connectivity bridge). B11 created the target with one module; B15
# lifted the full subtree here.
add_library(cc_orchestration)
target_sources(cc_orchestration
    PUBLIC FILE_SET CXX_MODULES FILES
        # Unified installer + seam consumers.
        orchestration/runtime_backends.cppm
        # B7/B8 MCP-connectivity bridge, rehomed from cc_bootstrap in B15.
        orchestration/mcp_connectivity.cppm
        # Lifted agent subtree (namespaces cc::tools retained byte-for-byte).
        orchestration/agent/agent_tool.cppm
        orchestration/agent/agent_run.cppm
        orchestration/agent/agent_resume.cppm
        orchestration/agent/agent_fork.cppm
        orchestration/agent/agent_sub_utils.cppm
        orchestration/agent/spawn_multi_agent.cppm
        # Lifted MCP/LSP tools.
        orchestration/tools/mcp_tool.cppm
        orchestration/tools/lsp_tool.cppm
)
target_sources(cc_orchestration
    PRIVATE
        # cc.orchestration.agent.utils implementation units.
        orchestration/agent/agent_sub_utils_json.cpp
        orchestration/agent/agent_sub_utils_config.cpp
        orchestration/agent/agent_sub_utils_tools_mcp.cpp
        orchestration/agent/agent_sub_utils_hooks.cpp
        orchestration/agent/agent_sub_utils_teammates.cpp
        orchestration/agent/agent_sub_utils_messages.cpp
        orchestration/agent/agent_sub_utils_budget.cpp
        # cc.orchestration.tools.mcp slot storage (loader B4 / snapshots B6).
        orchestration/tools/mcp_core_settings_loader.cpp
        orchestration/tools/mcp_snapshots_sink.cpp
        # cc.orchestration.runtime_backends implementation units.
        orchestration/runtime_backends_lsp.cpp
        orchestration/runtime_backends_mcp.cpp
        orchestration/runtime_backends_computer_use.cpp
        # c13b structured config runtime tool.
        orchestration/runtime_backends_config.cpp
)
# Every link goes to a strictly lower-ranked layer. cc_tools is the rank-8
# seam owner (runtime_backends.port / image_codec.port / runtime_registry);
# cc_services (7) backs MCP/LSP/image/api; cc_skills_core (5) the skill
# executor; cc_hooks (4) the connectivity projection; cc_config (1) the MCP
# config types.
target_link_libraries(cc_orchestration
    PUBLIC
        cc_tools
        cc_services
        cc_skills_core
        cc_hooks
        cc_config
        cc_utils
        cc_types
)
