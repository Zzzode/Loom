# ─── cc_ui_permissions: UI permission prompts (RFC 0002 F4) ───────────────
# The cc.ui.permissions.* area library: the permission prompt renderers
# (bash, computer-use, file edit, file write), the permission rule list and
# scope editor, the single-prompt flow, the shared permission components,
# the rules UI, and the shell helpers. Split out of the single cc_ui target
# so a body edit in this area recompiles only this area's objects (its own
# CXX.dd dyndep file), not the whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.permissions.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Permissions is UI9_RANK 6: it may link
# lower-ranked areas (visual/tools rank 1, foundation rank 2, chrome
# rank 3, prompt rank 4, widgets rank 5) and never a higher-ranked one.
add_library(cc_ui_permissions)
target_sources(cc_ui_permissions
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/permissions/permission_bash.cppm
        ui/permissions/permission_computer_use.cppm
        ui/permissions/permission_file_edit.cppm
        ui/permissions/permission_file_write.cppm
        ui/permissions/permission_rules_ui.cppm
        ui/permissions/permission_shell_helpers.cppm
        ui/permissions/permissions_components.cppm
        ui/permissions/permission_scope_editor.cppm
        ui/permissions/permission_rule_list.cppm
        ui/permissions/permission_single_prompt.cppm
)
# cc.ui.foundation.* (design_tokens), cc.ui.visual.* (code_highlight /
# file_edit_tool_diff / structured_diff), and cc.ui.widgets.custom_select.
# External deps: cc.types.types, cc.utils.* (file_edit / json /
# permissions_engine), and FTXUI (component / dom / screen headers).
# cc_std's `import std;` BMI arrives via the directory-level
# link_libraries(cc_std). Over-linking is safe (and matches the previous
# cc_ui.cmake behaviour).
target_link_libraries(cc_ui_permissions
    PUBLIC
        cc_ui_foundation
        cc_ui_visual
        cc_ui_widgets
        cc_types
        cc_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
