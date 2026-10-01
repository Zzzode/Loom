# ─── loom_ui_features: UI feature panels (RFC 0002 F4) ──────────────────────
# The cc.ui.features.* area library: the agent cards / shared widgets /
# wizard, the grove animation, the hooks UI, the plugin menus and panels
# (recommendation, hint, install flow, manage, marketplace browse,
# settings), and the live-teammates panel. Split out of the single loom_ui
# target so a body edit in this area recompiles only this area's objects
# (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.features.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Features is UI9_RANK 8: it may link lower-ranked
# areas (foundation rank 2, widgets rank 5) and never a higher-ranked one.
add_library(loom_ui_features)
target_sources(loom_ui_features
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/features/agents/agent_cards.cppm
        ui/features/agents/agent_shared_widgets.cppm
        ui/features/agents/agent_wizard.cppm
        ui/features/grove.cppm
        ui/features/hooks_ui.cppm
        ui/features/plugins/lsp_recommendation_menu.cppm
        ui/features/plugins/plugin_hint_menu.cppm
        ui/features/plugins/plugin_install_flow.cppm
        ui/features/plugins/plugin_manage_panel.cppm
        ui/features/plugins/plugin_marketplace_browse.cppm
        ui/features/plugins/plugin_settings_dialog.cppm
        ui/features/teams/live_teammates.cppm
)
# cc.ui.foundation.* (design_tokens / theme_provider / component_primitives /
# feature_dialog_protocol) and cc.ui.widgets.* (spinner_animations /
# custom_select). External deps: cc.tools.agent_color_manager,
# cc.teams.swarm.backends, cc.types.types, cc.commands.plugin_*
# (details_helpers / trust_text / ui_data / pagination_util / helpers), and
# FTXUI (component / dom / screen headers). loom_std's `import std;` BMI
# arrives via the directory-level link_libraries(loom_std). Over-linking is
# safe (and matches the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_features
    PUBLIC
        loom_ui_foundation
        loom_ui_widgets
        loom_tools
        loom_utils
        loom_types
        loom_commands
        loom_teams
        ftxui::screen
        ftxui::dom
        ftxui::component
)
