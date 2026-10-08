# ─── loom_ui_chrome: UI chrome (RFC 0002 F4) ──────────────────────────────────
# The loom.ui.chrome.* area library: terminal I/O, ANSI/SGR rendering, layout
# (fullscreen + panels), yoga, text measurement, and ink utilities. Split out
# of the single loom_ui target so a body edit in this area recompiles only this
# area's objects (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.chrome.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Chrome is UI9_RANK 3: it may link lower-ranked
# areas (foundation, rank 2) and never a higher-ranked one.
add_library(loom_ui_chrome)
target_sources(loom_ui_chrome
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/chrome/layout.cppm
        ui/chrome/fullscreen_layout.cppm
        ui/chrome/yoga.cppm
        ui/chrome/panels.cppm
        ui/chrome/ink_utils.cppm
        ui/chrome/text_measure.cppm
        ui/chrome/terminal_io.cppm
        ui/chrome/ansi_render.cppm
)
# Module implementation unit for loom.ui.chrome.ansi_render (RFC 0002
# phase F1 row 8): the ANSI/SGR -> FTXUI bodies, extracted verbatim
# from message_tool_result.cppm. Bodies in the impl unit keep the
# declarations-only BMI cheap and give fan-out = 1 on a body edit.
# Moved from loom_ui unchanged.
target_sources(loom_ui_chrome PRIVATE
    ui/chrome/ansi_render.cpp
)
# loom.ui.foundation.* (fullscreen_layout: theme_provider / design_tokens /
# design_figures) and loom.state.app_state (terminal). FTXUI arrives via GMF
# headers in the module prologue (screen_interactive, dom elements,
# component). loom_std's `import std;` BMI arrives via the directory-level
# link_libraries(loom_std). Over-linking is safe (and matches the previous
# loom_ui.cmake behaviour).
target_link_libraries(loom_ui_chrome
    PUBLIC
        loom_ui_foundation
        loom_state
        ftxui::screen
        ftxui::dom
        ftxui::component
)
