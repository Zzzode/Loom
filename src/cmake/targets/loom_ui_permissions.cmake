# ─── loom_ui_permissions: UI permission prompts (RFC 0002 F4) ───────────────
# The loom.ui.permissions.* area library: the unified single-prompt flow
# (single_prompt), the permission rule list and scope editor, the shared
# permission components, the computer-use prompt, the rules UI, and the
# shell helpers. The legacy per-tool prompts (bash / file_edit / file_write)
# were deleted — single_prompt covers all tool types via ToolDetail variants.
# Split out of the single loom_ui target so a body edit in this area
# recompiles only this area's objects (its own CXX.dd dyndep file), not the
# whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.permissions.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Permissions is UI9_RANK 6: it may link
# lower-ranked areas (visual/tools rank 1, foundation rank 2, chrome
# rank 3, prompt rank 4, widgets rank 5) and never a higher-ranked one.
add_library(loom_ui_permissions)
target_sources(loom_ui_permissions
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/permissions/permission_computer_use.cppm
        ui/permissions/permission_rules_ui.cppm
        ui/permissions/permission_shell_helpers.cppm
        ui/permissions/permissions_components.cppm
        ui/permissions/permission_scope_editor.cppm
        ui/permissions/permission_rule_list.cppm
        ui/permissions/permission_single_prompt.cppm
)
# Module implementation unit for loom.ui.permissions.rule_list: 61 function
# bodies moved out of the primary interface to keep the declarations-only
# BMI cheap (fan-out = 1 on a body edit).
target_sources(loom_ui_permissions PRIVATE
    ui/permissions/permission_rule_list.cpp
)
# loom.ui.foundation.* (design_tokens), loom.ui.visual.* (code_highlight /
# file_edit_tool_diff / structured_diff), and loom.ui.widgets.custom_select.
# External deps: loom.types.types, loom.utils.* (file_edit / json /
# permissions_engine), and FTXUI (component / dom / screen headers).
# loom_std's `import std;` BMI arrives via the directory-level
# link_libraries(loom_std). Over-linking is safe (and matches the previous
# loom_ui.cmake behaviour).
target_link_libraries(loom_ui_permissions
    PUBLIC
        loom_ui_foundation
        loom_ui_visual
        loom_ui_widgets
        loom_types
        loom_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
