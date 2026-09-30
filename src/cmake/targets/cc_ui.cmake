# ─── cc_ui: Terminal UI ───────────────────────────────────────────────────────
add_library(cc_ui)
target_sources(cc_ui
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/app/app.cppm
        ui/app/app_impl.cppm
        ui/app/app_dialog_registration.cppm
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
        ui/dialogs/all_renderers.cppm            # new: DialogRendererRegistry + all renderers
        ui/dialogs/bottom_renderers.cppm         # new: Bottom-band dialog renderers
        ui/dialogs/modal_renderers.cppm          # new: Overlay/Modal dialog renderers
        ui/dialogs/sandbox_permission.cppm       # new: SandboxPermission renderer
        ui/dialogs/triggers.cppm                 # new: Dialog trigger / invocation routing
        # M8 note: ManagedSettingsSecurity / FeedbackSurvey / GlobalSearch /
        # HistorySearch / PluginDialog / DiffDialog — FTXUI chrome not yet ported.
        # Do NOT add placeholder sources into FILE_SET CXX_MODULES: CMake 4.x
        # cascades "not scheduled for compilation" diagnostics across the
        # entire target if any listed source is missing.  Only add module
        # sources that exist AND contain real implementations.  Registration
        # lives in register_default_renderers() in default_renderers.cppm.
        ui/screens/doctor_screen.cppm
        ui/screens/doctor_dialog_registration.cppm  # RFC 0002 F2 row 4: doctor renderer registration (screens side; dialogs must not import screens)
        ui/screens/repl_state.cppm
        ui/screens/messages_store.cppm        # RFC 0002 F3: MessagesStore (message-list/scroll/chrome state shard)
        ui/screens/prompt_store.cppm          # RFC 0002 F3: PromptStore (prompt-input state shard)
        ui/screens/task_view_store.cppm       # RFC 0002 F3: TaskViewStore (spinner/task-notifications/agent-teammate live state shard)
        ui/screens/permission_store.cppm      # RFC 0002 F3: PermissionStore (permission-prompt state shard)
        ui/screens/dialog_store.cppm          # RFC 0002 F3: DialogStore (overlay dialogs / inline panels / M7 dialog queue shard)
        ui/screens/mcp_status_store.cppm      # RFC 0002 F3: McpStatusStore (MCP at-mention drained-queue shard)
        ui/screens/chrome_store.cppm          # RFC 0002 F3: ChromeStore (chrome/welcome-header/status-bar projection shard)
        ui/screens/repl_screen.cppm
        ui/screens/resume_screen.cppm
        ui/screens/log_selector.cppm              # UI23 — LogSelector (1574 → 1730 loc)
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
    ui/dialogs/hooks_dialog_renderer_impl.cpp
    ui/dialogs/plugin_dialog_renderer_impl.cpp
    # Module implementation units for cc.ui.screens.repl_screen
    # (RFC 0001 Phase C batch 9): message-row projection, unseen-divider /
    # scroll bounds, prompt buffer mutation, welcome/spinner, prompt
    # rendering, dialog-queue slots, full-screen layout, agents menu,
    # settings/trust/permission panels, and the component event factory.
    ui/screens/repl_screen_messages.cpp
    ui/screens/repl_screen_scroll.cpp
    ui/screens/repl_screen_prompt_buffer.cpp
    ui/screens/repl_screen_welcome.cpp
    ui/screens/repl_screen_prompt_render.cpp
    ui/screens/repl_screen_dialog_queue.cpp
    ui/screens/repl_screen_layout.cpp
    ui/screens/repl_screen_agents.cpp
    ui/screens/repl_screen_dialog_panels.cpp
    ui/screens/repl_screen_events.cpp
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
