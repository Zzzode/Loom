# ─── cc_tools: Tools System ───────────────────────────────────────────────────
add_library(cc_tools)
target_sources(cc_tools
    PUBLIC FILE_SET CXX_MODULES FILES
        tools/agent_color_manager.cppm
        tools/agent_constants.cppm
        tools/agent_memory.cppm
        tools/agent_memory_snapshot.cppm
        tools/agent_types.cppm
        # RFC-0001 B14 — agent worktree cleanup leaf (runtime_team_shared
        # imports this instead of the lifted cc.orchestration.agent facade).
        tools/agent_worktree.cppm
        tools/ask_user_tool.cppm
        tools/bash_permissions.cppm
        tools/bash_security.cppm
        tools/bash_tool.cppm
        tools/bash_result_formatting.cppm
        tools/built_in_agents.cppm
        tools/command_semantics.cppm
        tools/computer_use.cppm
        tools/cron_tool.cppm
        tools/file_edit_prompt.cppm
        tools/file_edit_tool.cppm
        tools/file_edit_types.cppm
        tools/file_read_tool.cppm
        tools/file_write_tool.cppm
        tools/glob_tool.cppm
        tools/grep_tool.cppm
        # RFC-0001 B11 — image codec callback leaf (cc_orchestration installs
        # the concrete cc.services.image-backed implementation).
        tools/image_codec_port.cppm
        tools/mcp_classify.cppm
        tools/notebook_tool.cppm
        tools/plan_mode_tool.cppm
        tools/powershell_tool.cppm
        tools/repl_tool.cppm
        tools/runtime_computer_use.cppm
        # RFC-0001 B12 — unified runtime-backends seam (cc_orchestration
        # installs the concrete SkillLoader-backed skill executor).
        tools/runtime_backends_port.cppm
        tools/runtime_message_delivery.cppm
        tools/runtime_registry.cppm
        tools/runtime_shared_utils.cppm
        tools/runtime_team_shared.cppm
        tools/script_diagnostics.cppm
        tools/script_tool.cppm
        tools/script_types.cppm
        tools/send_message_tool.cppm
        tools/skill_tool.cppm
        tools/sleep_tool.cppm
        tools/synthetic_output_tool.cppm
        tools/task_tool.cppm
        tools/team_create.cppm
        tools/team_delete.cppm
        tools/team_tool.cppm
        tools/todo_write_tool.cppm
        tools/tool.cppm
        tools/tool_display_names.cppm
        tools/tool_registry.cppm
        tools/tungsten_tool.cppm
        tools/web_browser_tool.cppm
        tools/web_fetch_tool.cppm
        tools/web_search_tool.cppm
        tools/workflow_tool.cppm
        tools/worktree_tool.cppm
        tools/agent_runtime.cppm
        tools/agent_display.cppm
        tools/bash_validation.cppm
        tools/destructive_command_warning.cppm
        tools/mode_validation.cppm
        tools/path_validation.cppm
        tools/readonly_validation.cppm
        tools/should_use_sandbox.cppm
        tools/sed_edit_parser.cppm
        tools/sed_validation.cppm
        tools/script_typecheck.cppm
        tools/script_primitives.cppm
        tools/feature_flags.cppm
        # Phase 3-B/C real tool primitives: bash exec + file I/O + glob/grep
        tools/bash/impl_bash.cppm
        tools/files/impl_files.cppm
        # RFC-0001 Phase D B3 — tools.support move (files stay at
        # src/utils/tools/; FILE_SET membership only, per the types move
        # precedent in B1).
        utils/tools/script_tool_enabled.cppm
        utils/tools/tool_helpers.cppm
)
# RFC 0001 Phase C — runtime_registry module implementation units. Never add
# these to the FILE_SET CXX_MODULES list above: they are module impl units
# (`module cc.tools.runtime_registry;`), not interface units.
target_sources(cc_tools
    PRIVATE
        tools/runtime_registry_json.cpp
        tools/runtime_registry_executors.cpp
        tools/runtime_registry_native_agents.cpp
        tools/runtime_registry_skills.cpp
        tools/runtime_registry_dispatch.cpp
        tools/runtime_registry_team_dispatch.cpp
        tools/runtime_registry_register.cpp
        # RFC-0001 B15 — unified runtime-backends seam slots
        # (`module cc.tools.runtime_backends.port;`).
        tools/runtime_backends_port.cpp
        # RFC 0001 Phase C batch 3 — agent_runtime module implementation units
        # (`module cc.tools.agent_runtime;`); same PRIVATE-only discipline.
        tools/agent_runtime_text_impl.cpp
        tools/agent_runtime_yaml_impl.cpp
        tools/agent_runtime_json_impl.cpp
        tools/agent_runtime_builtin_impl.cpp
        tools/agent_runtime_sidechain_impl.cpp
        tools/agent_runtime_store_impl.cpp
        # RFC-0001 B14 — cc.tools.agent_worktree implementation unit.
        tools/agent_worktree.cpp
)
# RFC-0001 B15 final link set: the lifted agent/mcp/lsp/computer-use TUs
# left for cc_orchestration; zero remaining cc.tools.* modules import
# cc.services.* / cc.config.* / cc.hooks.* (grep-verified on the spike
# tree). The 3 cc_skills_core edges are the skills file_access.port
# contracts.
target_link_libraries(cc_tools
    PUBLIC
        cc_utils
        cc_types
        cc_skills_core
        yyjson
        uv_a
)
if(APPLE)
    target_link_libraries(cc_tools PUBLIC "-framework ApplicationServices")
endif()

target_link_libraries(cc_query PUBLIC cc_tools cc_hooks cc_session cc_memdir cc_services)
