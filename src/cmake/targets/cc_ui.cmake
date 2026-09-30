# ─── cc_ui: Terminal UI ───────────────────────────────────────────────────────
add_library(cc_ui)
target_sources(cc_ui
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/app/app.cppm
        ui/app/app_impl.cppm
        ui/app/app_dialog_registration.cppm
)
# Module implementation units for cc.ui.app_dialog_registration — one per
# dialog-renderer aggregator, so no single TU imports more than one aggregator's
# closure (importing all four at once crashes Clang codegen). This keeps the
# ~44 dialog implementations out of app.cppm's BMI (source-location budget).
# See app_dialog_registration.cppm / *_impl.cpp for the rationale.
target_sources(cc_ui PRIVATE
    ui/app/app_autocomplete.cpp
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
    # Module implementation unit for cc.ui.app.app_dialog_registration
    # (RFC 0002 F2 row 6): the feature-dialog factory registration (agent
    # wizard, plugin install wizard, plugin trust dialog). The concrete
    # static_pointer_cast of the erased request lives ONLY here, so the
    # feature and dialogs modules never name each other's types.
    ui/app/app_feature_dialog_registration.cpp
    ui/app/app_message_projection.cpp
    ui/app/app_store_bridge.cpp
    ui/app/app_run.cpp
    ui/app/app_team.cpp
    ui/app/app_settings.cpp
    # Module implementation units for cc.ui.app.app (RFC 0001 Phase C
    # batch 1): env/text/UTF helpers + skills-menu formatting, moved out of
    # app.cppm for edit isolation. Textual-std (LLVM #184957) — see the
    # header comment in each file and CMakeLists.txt:283-292.
    ui/app/app_helpers.cpp
    ui/app/app_skills_menu.cpp
    # Module implementation units for cc.ui.app.app (RFC 0001 Phase C
    # batch 2): animation ticker + render-event post, local-command /
    # local-JSX rows. Textual-std (LLVM #184957) — see the header comment
    # in each file and CMakeLists.txt:283-292.
    ui/app/app_animation.cpp
    ui/app/app_local_command.cpp
    # Module implementation unit for cc.ui.app.app (RFC 0002 F3 Finalize):
    # the 28 AppAdapter *_for_testing seam bodies, moved out of app.cppm so
    # the inline-def ratchet re-freezes at the single composition body
    # (set_screen). Textual-std (LLVM #184957) — see the header comment.
    ui/app/app_testing_seams.cpp
)
target_link_libraries(cc_ui
    PUBLIC
        # RFC 0002 F4: the cc.ui.foundation.* area now lives in its own
        # library; cc_ui links it PUBLIC so the remaining areas can still
        # import cc.ui.foundation.* during the staged split.
        cc_ui_foundation
        # RFC 0002 F4: the cc.ui.visual.* area (markdown, code highlight,
        # diffs) now lives in its own library; linked PUBLIC so the
        # remaining areas can still import cc.ui.visual.* during the split.
        cc_ui_visual
        # RFC 0002 F4: the cc.ui.tools.* area (per-tool UI render registry)
        # now lives in its own library; linked PUBLIC so the remaining
        # areas can still import cc.ui.tools.* during the staged split.
        cc_ui_tools
        # RFC 0002 F4: the cc.ui.chrome.* area (terminal I/O, layout, ANSI
        # rendering, text measurement) now lives in its own library; linked
        # PUBLIC so the remaining areas can still import cc.ui.chrome.*
        # during the staged split.
        cc_ui_chrome
        # RFC 0002 F4: the cc.ui.prompt.* area (prompt input, autocomplete,
        # at-attachments, fuzzy ranking, vim input) now lives in its own
        # library; linked PUBLIC so the remaining areas can still import
        # cc.ui.prompt.* during the staged split.
        cc_ui_prompt
        # RFC 0002 F4: the cc.ui.widgets.* area (reusable FTXUI controls:
        # spinner, stats, tag tabs, text input, custom select, dev bar,
        # PR badge, passes, fast icon) now lives in its own library;
        # linked PUBLIC so the remaining areas can still import
        # cc.ui.widgets.* during the staged split.
        cc_ui_widgets
        # RFC 0002 F4: the cc.ui.permissions.* area (permission prompt
        # renderers, rule list, scope editor, single-prompt flow) now lives
        # in its own library; linked PUBLIC so the remaining areas can still
        # import cc.ui.permissions.* during the staged split.
        cc_ui_permissions
        # RFC 0002 F4: the cc.ui.messages.* area (message row + per-type
        # renderers, pipeline, virtualized list) now lives in its own
        # library; linked PUBLIC so the remaining areas can still import
        # cc.ui.messages.* during the staged split.
        cc_ui_messages
        # RFC 0002 F4: the cc.ui.features.* area (agent cards/wizard, grove,
        # hooks UI, plugin menus/panels, live teammates) now lives in its
        # own library; linked PUBLIC so the remaining areas can still import
        # cc.ui.features.* during the staged split.
        cc_ui_features
        # RFC 0002 F4: the cc.ui.dialogs.* area (dialog system/frame,
        # renderer registries, per-dialog renderers, trust utils, wizard
        # adapter, triggers) now lives in its own library; linked PUBLIC so
        # the remaining areas (screens, app) can still import
        # cc.ui.dialogs.* during the staged split.
        cc_ui_dialogs
        # RFC 0002 F4: the cc.ui.screens.* area (REPL/resume/doctor/
        # log-selector screens, ReplScreenState, the seven F3 stores) now
        # lives in its own library; linked PUBLIC so the remaining area
        # (app) can still import cc.ui.screens.* during the staged split.
        cc_ui_screens
        cc_utils
        cc_types
        cc_query
        cc_commands
        # RFC-0001 B15: the at_attachments/autocomplete impl TUs that import
        # cc.orchestration.tools.mcp moved to cc_ui_prompt in the F4 split;
        # cc_orchestration also arrives transitively via cc_ui_prompt, but is
        # kept explicit (over-linking is safe).
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
