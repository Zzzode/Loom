# ─── cc_ui_screens: UI screens (RFC 0002 F4) ─────────────────────────────
# The cc.ui.screens.* area library: the REPL screen and its projection
# impl units, the resume / doctor / log-selector screens, the doctor
# dialog registration, the ReplScreenState shard, and the seven F3 stores
# (messages, prompt, task-view, permission, dialog, mcp-status, chrome).
# Split out of the single cc_ui target so a body edit in this area
# recompiles only this area's objects (its own CXX.dd dyndep file), not the
# whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.screens.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Screens is UI9_RANK 10: it may link lower-ranked
# areas (visual rank 1, foundation rank 2, chrome rank 3, prompt rank 4,
# widgets rank 5, permissions rank 6, messages rank 7, features rank 8,
# dialogs rank 9) and never a higher-ranked one (app, rank 11).
add_library(cc_ui_screens)
target_sources(cc_ui_screens
    PUBLIC FILE_SET CXX_MODULES FILES
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
# Module implementation units for cc.ui.screens.repl_screen
# (RFC 0001 Phase C batch 9): message-row projection, unseen-divider /
# scroll bounds, prompt buffer mutation, welcome/spinner, prompt
# rendering, dialog-queue slots, full-screen layout, agents menu,
# settings/trust/permission panels, and the component event factory.
# Moved from cc_ui unchanged; they implement the module interface owned
# by this target.
target_sources(cc_ui_screens PRIVATE
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
# cc.ui.visual.markdown (StreamingMarkdown ptr field), cc.ui.foundation.*
# (design tokens / figures / logo / theme / ui_types / declared_cursor),
# cc.ui.chrome.* (fullscreen_layout / ink_utils), cc.ui.prompt.*
# (prompt_input_footer / vim_input / prompt_stash_notice /
# placeholder_cascade), cc.ui.widgets.text_input, cc.ui.permissions.*
# (single_prompt / permission_bash / permission_file_edit /
# permission_file_write), cc.ui.messages.* (message_row / messages_list /
# virtual_list / per-type renderers / message_tool_result),
# cc.ui.features.* (agent_cards / live_teammates / agent_wizard), and
# cc.ui.dialogs.* (system / trust_dialog / trust_utils / settings_dialog /
# cost_threshold_dialog). External deps: cc.config.config,
# cc.constants.spinner_verbs, cc.session.history, cc.tools.agent_display,
# cc.types.types, cc.serdes.json / terminal_helpers, and FTXUI
# (component / dom / screen headers). cc_std's `import std;` BMI arrives
# via the directory-level link_libraries(cc_std). Over-linking is safe
# (and matches the previous cc_ui.cmake behaviour).
target_link_libraries(cc_ui_screens
    PUBLIC
        cc_ui_foundation
        cc_ui_visual
        cc_ui_chrome
        cc_ui_prompt
        cc_ui_widgets
        cc_ui_permissions
        cc_ui_messages
        cc_ui_features
        cc_ui_dialogs
        cc_config
        cc_constants
        cc_session
        cc_tools
        cc_types
        cc_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
