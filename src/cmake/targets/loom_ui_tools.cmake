# ─── loom_ui_tools: UI tool-render registry (RFC 0002 F4) ─────────────────────
# The loom.ui.tools.* area library: the per-tool UI render-function registry
# (user-facing name, inline summary, tag, progress, queued, search-text
# extraction) and its 16 tool-specific renderers. Split out of the single
# loom_ui target so a body edit in this area recompiles only this area's
# objects (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.tools.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Tools is UI9_RANK 1 (a pure leaf: zero loom.ui
# imports and zero external module imports — every renderer consumes only
# std:: types through the registry's std::function bundle), so it links no
# other loom_ui_<area> library and no loom.* library. loom_std's `import std;`
# BMI arrives via the directory-level link_libraries(loom_std).
add_library(loom_ui_tools)
target_sources(loom_ui_tools
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/tools/tool_ui_registry.cppm
        ui/tools/tool_ui_init.cppm
        ui/tools/tool_ui_generic.cppm
        ui/tools/tool_ui_bash.cppm
        ui/tools/tool_ui_file_edit.cppm
        ui/tools/tool_ui_file_write.cppm
        ui/tools/tool_ui_file_read.cppm
        ui/tools/tool_ui_grep.cppm
        ui/tools/tool_ui_glob.cppm
        ui/tools/tool_ui_web_fetch.cppm
        ui/tools/tool_ui_web_search.cppm
        ui/tools/tool_ui_skill.cppm
        ui/tools/tool_ui_agent.cppm
        ui/tools/tool_ui_task.cppm
        ui/tools/tool_ui_mcp.cppm
        ui/tools/tool_ui_lsp.cppm
        ui/tools/tool_ui_longtail.cppm
)
