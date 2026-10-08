# ─── loom_ui: Terminal UI (RFC 0002 F4 aggregate) ──────────────────────────────
# All twelve loom.ui.<area> module areas live in their own libraries
# (loom_ui_<area>), grouped by MODULE-NAME area (export module
# loom.ui.<area>.*), not by directory — name/path decoupling means the
# grouping rule is stated, not inferred from the tree. loom_ui is a
# source-less INTERFACE aggregate linking them PUBLIC so the upper layers
# (loom, loom_server, the tests, …) keep a single loom_ui link edge. The
# split bounds recompile fan-out: each area owns its CXX.dd dyndep file,
# so a body edit in one area regenerates only that area's dyndep and
# recompiles only that area's objects, not the whole loom_ui closure.
add_library(loom_ui INTERFACE)
target_link_libraries(loom_ui
    INTERFACE
        # UI9_RANK ascending (leaves first), matching the include order in
        # src/CMakeLists.txt: visual/tools rank 1, foundation rank 2, chrome
        # rank 3, prompt rank 4, widgets rank 5, permissions rank 6, messages
        # rank 7, features rank 8, dialogs rank 9, screens rank 10, app rank 11.
        loom_ui_visual
        loom_ui_tools
        loom_ui_foundation
        loom_ui_chrome
        loom_ui_prompt
        loom_ui_widgets
        loom_ui_permissions
        loom_ui_messages
        loom_ui_features
        loom_ui_dialogs
        loom_ui_screens
        loom_ui_app
        loom_utils
        loom_types
        loom_query
        loom_commands
        # RFC-0001 B15: the at_attachments/autocomplete impl TUs that import
        # loom.orchestration.tools.mcp live in loom_ui_prompt; loom_orchestration
        # also arrives transitively via loom_ui_prompt, but is kept explicit
        # (over-linking is safe).
        loom_orchestration
        loom_vim
        loom_hooks
        loom_plugins
        loom_session
        loom_history
        loom_skills            # SkillRegistry::on_skills_changed for dynamic skill refresh
        # loom_ui consumed loom.services.* transitively via loom_hooks; the explicit
        # link is correct (no cycle: loom_services never imports loom.ui.*).
        loom_services
        ftxui::screen
        ftxui::dom
        ftxui::component
)
