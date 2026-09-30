# ─── cc_ui: Terminal UI (RFC 0002 F4 aggregate) ──────────────────────────────
# All twelve cc.ui.<area> module areas live in their own libraries
# (cc_ui_<area>), grouped by MODULE-NAME area (export module
# cc.ui.<area>.*), not by directory — name/path decoupling means the
# grouping rule is stated, not inferred from the tree. cc_ui is a
# source-less INTERFACE aggregate linking them PUBLIC so the upper layers
# (loom, cc_server, the tests, …) keep a single cc_ui link edge. The
# split bounds recompile fan-out: each area owns its CXX.dd dyndep file,
# so a body edit in one area regenerates only that area's dyndep and
# recompiles only that area's objects, not the whole cc_ui closure.
add_library(cc_ui INTERFACE)
target_link_libraries(cc_ui
    INTERFACE
        # UI9_RANK ascending (leaves first), matching the include order in
        # src/CMakeLists.txt: visual/tools rank 1, foundation rank 2, chrome
        # rank 3, prompt rank 4, widgets rank 5, permissions rank 6, messages
        # rank 7, features rank 8, dialogs rank 9, screens rank 10, app rank 11.
        cc_ui_visual
        cc_ui_tools
        cc_ui_foundation
        cc_ui_chrome
        cc_ui_prompt
        cc_ui_widgets
        cc_ui_permissions
        cc_ui_messages
        cc_ui_features
        cc_ui_dialogs
        cc_ui_screens
        cc_ui_app
        cc_utils
        cc_types
        cc_query
        cc_commands
        # RFC-0001 B15: the at_attachments/autocomplete impl TUs that import
        # cc.orchestration.tools.mcp live in cc_ui_prompt; cc_orchestration
        # also arrives transitively via cc_ui_prompt, but is kept explicit
        # (over-linking is safe).
        cc_orchestration
        cc_vim
        cc_hooks
        cc_plugins
        cc_session
        cc_history
        cc_skills            # SkillRegistry::on_skills_changed for dynamic skill refresh
        # cc_ui consumed cc.services.* transitively via cc_hooks; the explicit
        # link is correct (no cycle: cc_services never imports cc.ui.*).
        cc_services
        ftxui::screen
        ftxui::dom
        ftxui::component
)
