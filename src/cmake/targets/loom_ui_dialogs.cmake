# ─── loom_ui_dialogs: UI dialogs (RFC 0002 F4) ─────────────────────────────
# The cc.ui.dialogs.* area library: the dialog system and frame, the
# default / bottom-band / modal renderer registries, the per-dialog
# renderers (hooks, plugin, trust, wizard, settings, prompt, quick-open,
# cost-threshold, sandbox-permission, elicitation, MCP), the trust utils,
# the feature-wizard adapter, and the dialog trigger / invocation routing.
# Split out of the single loom_ui target so a body edit in this area
# recompiles only this area's objects (its own CXX.dd dyndep file), not the
# whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.dialogs.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Dialogs is UI9_RANK 9: it may link lower-ranked
# areas (foundation rank 2, widgets rank 5, permissions rank 6, features
# rank 8) and never a higher-ranked one.
add_library(loom_ui_dialogs)
target_sources(loom_ui_dialogs
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/dialogs/dialog_system.cppm
        ui/dialogs/dialog_frame.cppm
        ui/dialogs/dialog_default_renderers.cppm
        ui/dialogs/hooks_dialog_renderer.cppm
        ui/dialogs/elicitation_dialog.cppm
        ui/dialogs/mcp_dialogs.cppm
        ui/dialogs/plugin_dialog.cppm
        ui/dialogs/plugin_dialog_renderer.cppm
        ui/dialogs/prompt_dialog.cppm
        ui/dialogs/quick_open.cppm
        ui/dialogs/settings_dialog.cppm
        ui/dialogs/trust_dialog.cppm
        ui/dialogs/trust_utils.cppm
        ui/dialogs/wizard_dialog.cppm
        ui/dialogs/feature_wizard_adapter.cppm  # RFC 0002 F2 row 6: neutral FeatureWizardRequest -> wizard_dialog adapter (dialogs side; imports only the protocol leaf)
        ui/dialogs/cost_threshold_dialog.cppm
        ui/dialogs/all_renderers.cppm            # DialogRendererRegistry + all renderers
        ui/dialogs/bottom_renderers.cppm         # Bottom-band dialog renderers
        ui/dialogs/modal_renderers.cppm          # Overlay/Modal dialog renderers
        ui/dialogs/sandbox_permission.cppm       # SandboxPermission renderer
        ui/dialogs/triggers.cppm                 # Dialog trigger / invocation routing
        # M8 note: ManagedSettingsSecurity / FeedbackSurvey / GlobalSearch /
        # HistorySearch / PluginDialog / DiffDialog — FTXUI chrome not yet ported.
        # Do NOT add placeholder sources into FILE_SET CXX_MODULES: CMake 4.x
        # cascades "not scheduled for compilation" diagnostics across the
        # entire target if any listed source is missing.  Only add module
        # sources that exist AND contain real implementations.  Registration
        # lives in register_default_renderers() in default_renderers.cppm.
)
# Module implementation units for cc.ui.dialogs.hooks_renderer and
# cc.ui.dialogs.plugin_dialog_renderer: the renderer bodies, kept out of
# the interface BMIs for edit isolation. Moved from loom_ui unchanged; they
# implement the module interfaces owned by this target.
target_sources(loom_ui_dialogs PRIVATE
    ui/dialogs/hooks_dialog_renderer_impl.cpp
    ui/dialogs/plugin_dialog_renderer_impl.cpp
)
# cc.ui.foundation.* (design_tokens / theme_provider / component_primitives
# / feature_dialog_protocol), cc.ui.widgets.custom_select,
# cc.ui.permissions.* (single_prompt / components), and cc.ui.features.*
# (hooks_ui + the plugin menus/panels the plugin dialog embeds). External
# deps: cc.commands.plugin_* (details_helpers / error_formatting / helpers
# / pagination_util / trust_text / ui_data / manage_plugins / plugin_trust),
# cc.config.config, cc.constants.product, cc.plugins.plugin,
# cc.services.team_memory.secret_scanner, cc.tools.registry, cc.types.types,
# cc.utils.* (bash_security / plugin_marketplace), cc.hooks.config /
# cc.hooks.registry (hooks dialog config + registry), and FTXUI
# (component / dom / screen headers).
# loom_std's `import std;` BMI arrives via the directory-level
# link_libraries(loom_std). Over-linking is safe (and matches the previous
# loom_ui.cmake behaviour).
target_link_libraries(loom_ui_dialogs
    PUBLIC
        loom_ui_foundation
        loom_ui_widgets
        loom_ui_permissions
        loom_ui_features
        loom_commands
        loom_config
        loom_constants
        loom_hooks
        loom_plugins
        loom_services
        loom_tools
        loom_types
        loom_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
