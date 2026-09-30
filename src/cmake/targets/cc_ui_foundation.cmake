# ─── cc_ui_foundation: UI foundation (RFC 0002 F4) ────────────────────────────
# The cc.ui.foundation.* area library: design tokens, theme, figures, glyphs,
# component primitives, and the feature-dialog protocol leaf. Split out of the
# single cc_ui target so a body edit in this area recompiles only this area's
# objects (its own CXX.dd dyndep file), not the whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.foundation.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Foundation is UI9_RANK 2 (a leaf; only visual/tools
# rank lower), so it links no other cc_ui_<area> library.
add_library(cc_ui_foundation)
target_sources(cc_ui_foundation
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/foundation/component_primitives.cppm
        ui/foundation/components_figures.cppm
        ui/foundation/declared_cursor.cppm
        ui/foundation/design_figures.cppm
        ui/foundation/design_logo.cppm
        ui/foundation/design_tokens.cppm
        ui/foundation/feature_dialog_protocol.cppm
        ui/foundation/logo.cppm
        ui/foundation/logo_v2.cppm
        ui/foundation/theme_provider.cppm
        ui/foundation/ui_types.cppm
)
# External deps only: cc.vim.vim_types (ui_types.cppm) + FTXUI. cc_std's
# `import std;` BMI arrives via the directory-level link_libraries(cc_std).
# Over-linking is safe (and matches the previous cc_ui.cmake behaviour).
target_link_libraries(cc_ui_foundation
    PUBLIC
        cc_vim
        ftxui::screen
        ftxui::dom
        ftxui::component
)
