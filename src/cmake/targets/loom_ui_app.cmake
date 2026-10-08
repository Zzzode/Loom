# ─── loom_ui_app: UI composition root (RFC 0002 F4) ─────────────────────
# The loom.ui.app.* area library: the App composition root (app.cppm), its
# :impl partition (app_impl.cppm), the dialog-renderer registration
# aggregator (app_dialog_registration.cppm) plus its impl units, and the
# loom.ui.app.app impl TUs (env/text/UTF helpers, skills-menu formatting,
# animation ticker + render-event post, local-command / local-JSX rows,
# the AppAdapter *_for_testing seams, …). Split out of the single loom_ui
# target so a body edit in this area recompiles only this area's objects
# (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.app.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. App is UI9_RANK 11 (the composition root): it
# may link every lower-ranked area (visual/tools rank 1, foundation rank 2,
# chrome rank 3, prompt rank 4, widgets rank 5, permissions rank 6,
# messages rank 7, features rank 8, dialogs rank 9, screens rank 10) and
# nothing ranks above it.
add_library(loom_ui_app)
target_sources(loom_ui_app
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/app/app.cppm
        ui/app/app_impl.cppm
        ui/app/app_dialog_registration.cppm
        # RFC 0001 Phase D B5a: moved from loom_utils (src/utils/statusline/).
        # Imports loom.serdes.json + loom.hooks.execution; loom_ui_app links
        # loom_utils and loom_hooks PUBLIC, so deps are satisfied.
        ui/app/statusline_runner.cppm
        # P3-1c: async statusline worker (jthread + debounce/memo state)
        # extracted from AppAdapter.
        ui/app/statusline_coordinator.cppm
        # P3-1a: teammate inbox/permission state extracted from AppAdapter.
        ui/app/teammate_coordinator.cppm
        # P4-1b: async clipboard paste state + worker extracted from AppAdapter.
        ui/app/paste_coordinator.cppm
        # P4-1d: tool-permission + MCP-elicitation blocking-response state.
        ui/app/permission_coordinator.cppm
)
# Module implementation units for loom.ui.app.app_dialog_registration — one per
# dialog-renderer aggregator, so no single TU imports more than one aggregator's
# closure (importing all four at once crashes Clang codegen). This keeps the
# ~44 dialog implementations out of app.cppm's BMI (source-location budget).
# See app_dialog_registration.cppm / *_impl.cpp for the rationale.
target_sources(loom_ui_app PRIVATE
    ui/app/app_autocomplete.cpp
    ui/app/app_render_event.cpp
    ui/app/app_text_selection.cpp
    ui/app/app_extra_methods.cpp
    ui/app/app_constructor.cpp
    ui/app/app_handle_submit.cpp
    ui/app/app_agent_menu.cpp
    ui/app/app_prompt_suggestion_wiring.cpp
    ui/app/app_dialog_registration_default.cpp
    ui/app/app_dialog_registration_modal.cpp
    ui/app/app_dialog_registration_bottom.cpp
    ui/app/app_dialog_registration_all.cpp
    ui/app/app_dialog_registration_hooks.cpp
    ui/app/app_dialog_registration_teams.cpp
    # Module implementation unit for loom.ui.app.app_dialog_registration
    # (RFC 0002 F2 row 6): the feature-dialog factory registration (agent
    # wizard, plugin install wizard, plugin trust dialog). The concrete
    # static_pointer_cast of the erased request lives ONLY here, so the
    # feature and dialogs modules never name each other's types.
    ui/app/app_feature_dialog_registration.cpp
    ui/app/app_message_projection.cpp
    ui/app/app_store_bridge.cpp
    ui/app/app_run.cpp
    ui/app/app_team.cpp
    # P3-1a: TeammateCoordinator impl unit (owns the inbox/permission PIMPL).
    ui/app/teammate_coordinator.cpp
    # P4-1b: PasteCoordinator impl unit (SpawnPasteWorker background thread).
    ui/app/paste_coordinator.cpp
    ui/app/app_settings.cpp
    # Module implementation units for loom.ui.app.app (RFC 0001 Phase C
    # batch 1): env/text/UTF helpers + skills-menu formatting, moved out of
    # app.cppm for edit isolation. Textual-std (LLVM #184957) — see the
    # header comment in each file and CMakeLists.txt:283-292.
    ui/app/app_helpers.cpp
    ui/app/app_skills_menu.cpp
    # Module implementation units for loom.ui.app.app (RFC 0001 Phase C
    # batch 2): animation ticker + render-event post, local-command /
    # local-JSX rows. Textual-std (LLVM #184957) — see the header comment
    # in each file and CMakeLists.txt:283-292.
    ui/app/app_animation.cpp
    ui/app/app_local_command.cpp
    # Module implementation unit for loom.ui.app.app (RFC 0002 F3 Finalize):
    # the 28 AppAdapter *_for_testing seam bodies, moved out of app.cppm so
    # the inline-def ratchet re-freezes at the single composition body
    # (set_screen). Textual-std (LLVM #184957) — see the header comment.
    ui/app/app_testing_seams.cpp
)
# loom.ui.app.* imports loom.ui.tools.init, loom.ui.visual.markdown,
# loom.ui.foundation.* (declared_cursor / design_figures /
# feature_dialog_protocol), loom.ui.prompt.* (at_attachments /
# autocomplete_sources / file_index / fuzzy_rank_nucleo /
# prompt_input_footer), loom.ui.permissions.* (permission_computer_use /
# single_prompt), loom.ui.messages.* (message_pipeline), loom.ui.features.* (agent_cards / agent_shared_widgets /
# live_teammates), loom.ui.dialogs.* (system / per-renderer registries /
# trust_dialog / trust_utils / triggers / feature_wizard_adapter /
# elicitation / plugin_dialog_renderer), and loom.ui.screens.* (repl_screen /
# repl_state / doctor_dialog_registration / the seven F3 stores). External
# deps: loom.commands.*, loom.constants.constants, loom.hooks.*, loom.query.
# query_engine, loom.services.* (mcp at_mention / elicitation /
# prompt_suggestion), loom.skills.load_skills_dir, loom.state.* (app_state /
# store), loom.tools.* (agent_display / agent_runtime / ask_user),
# loom.types.*, loom.utils.*, loom.vim.vim_mode, and FTXUI (component / dom /
# screen headers). loom_std's `import std;` BMI arrives via the
# directory-level link_libraries(loom_std). Over-linking is safe (and matches
# the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_app
    PUBLIC
        loom_ui_visual
        loom_ui_tools
        loom_ui_foundation
        loom_ui_prompt
        loom_ui_permissions
        loom_ui_messages
        loom_ui_features
        loom_ui_dialogs
        loom_ui_screens
        loom_commands
        loom_config
        loom_constants
        loom_hooks
        loom_query
        loom_services
        loom_skills
        loom_state
        loom_tools
        loom_types
        loom_utils
        loom_vim
        loom_teams
        ftxui::screen
        ftxui::dom
        ftxui::component
)
