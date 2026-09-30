# ─── cc_ui_chrome: UI chrome (RFC 0002 F4) ──────────────────────────────────
# The cc.ui.chrome.* area library: terminal I/O, ANSI/SGR rendering, layout
# (fullscreen + panels), yoga, text measurement, and ink utilities. Split out
# of the single cc_ui target so a body edit in this area recompiles only this
# area's objects (its own CXX.dd dyndep file), not the whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.chrome.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Chrome is UI9_RANK 3: it may link lower-ranked
# areas (foundation, rank 2) and never a higher-ranked one.
add_library(cc_ui_chrome)
target_sources(cc_ui_chrome
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/chrome/layout.cppm
        ui/chrome/fullscreen_layout.cppm
        ui/chrome/yoga.cppm
        ui/chrome/panels.cppm
        ui/chrome/ink_utils.cppm
        ui/chrome/text_measure.cppm
        ui/chrome/terminal_io.cppm
        ui/chrome/ansi_render.cppm
        ui/chrome/terminal.cppm
)
# Module implementation unit for cc.ui.chrome.ansi_render (RFC 0002
# phase F1 row 8): the ANSI/SGR -> FTXUI bodies, extracted verbatim
# from message_tool_result.cppm. Bodies in the impl unit keep the
# declarations-only BMI cheap and give fan-out = 1 on a body edit.
# Moved from cc_ui unchanged.
target_sources(cc_ui_chrome PRIVATE
    ui/chrome/ansi_render.cpp
)
# cc.ui.foundation.* (fullscreen_layout: theme_provider / design_tokens /
# design_figures) and cc.state.app_state (terminal). FTXUI arrives via GMF
# headers in the module prologue (screen_interactive, dom elements,
# component). cc_std's `import std;` BMI arrives via the directory-level
# link_libraries(cc_std). Over-linking is safe (and matches the previous
# cc_ui.cmake behaviour).
target_link_libraries(cc_ui_chrome
    PUBLIC
        cc_ui_foundation
        cc_state
        ftxui::screen
        ftxui::dom
        ftxui::component
)
