# ─── loom_ui_foundation: UI foundation (RFC 0002 F4) ────────────────────────────
# The loom.ui.foundation.* area library: design tokens, theme, figures, glyphs,
# component primitives, and the feature-dialog protocol leaf. Split out of the
# single loom_ui target so a body edit in this area recompiles only this area's
# objects (its own CXX.dd dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.foundation.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Foundation is UI9_RANK 2 (a leaf; only visual/tools
# rank lower), so it links no other loom_ui_<area> library.
add_library(loom_ui_foundation)
target_sources(loom_ui_foundation
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/foundation/clock.cppm
        ui/foundation/component_primitives.cppm
        ui/foundation/components_figures.cppm
        ui/foundation/declared_cursor.cppm
        ui/foundation/design_figures.cppm
        ui/foundation/design_tokens.cppm
        ui/foundation/feature_dialog_protocol.cppm
        ui/foundation/logo.cppm
        ui/foundation/theme_provider.cppm
        ui/foundation/ui_types.cppm
)
# clock.cpp is the impl unit for loom.ui.foundation.clock (RFC 0003 §8.3):
# the steady_now() override storage and bodies live here, not in the .cppm,
# so the inline-def ratchet sees zero inline bodies in the interface.
target_sources(loom_ui_foundation PRIVATE
    ui/foundation/clock.cpp
)
# External deps only: loom.vim.vim_types (ui_types.cppm) + FTXUI. loom_std's
# `import std;` BMI arrives via the directory-level link_libraries(loom_std).
# Over-linking is safe (and matches the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_foundation
    PUBLIC
        loom_vim
        ftxui::screen
        ftxui::dom
        ftxui::component
)
