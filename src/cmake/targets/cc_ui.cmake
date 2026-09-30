# ─── cc_ui: Terminal UI (RFC 0002 F4 aggregate) ──────────────────────────────
# All twelve cc.ui.<area> module areas now live in their own libraries
# (cc_ui_<area>); cc_ui is a source-less INTERFACE aggregate that links them
# PUBLIC so the upper layers (loom, cc_server, the tests, …) keep a single
# cc_ui link edge. The split bounds recompile fan-out: a body edit in one
# area regenerates only that area's CXX.dd dyndep file and recompiles only
# that area's objects, not the whole cc_ui closure. Grouped by MODULE-NAME
# area (export module cc.ui.<area>.*), not by directory — name/path
# decoupling means the grouping rule is stated, not inferred from the tree.
add_library(cc_ui INTERFACE)
target_link_libraries(cc_ui
    INTERFACE
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
        # RFC 0002 F4: the cc.ui.app.* area (the App composition root, its
        # :impl partition, the dialog-renderer registration aggregator, and
        # the cc.ui.app.app impl TUs) now lives in its own library — the
        # last area split out of cc_ui.
        cc_ui_app
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
