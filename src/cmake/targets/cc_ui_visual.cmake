# ─── cc_ui_visual: UI visual rendering (RFC 0002 F4) ─────────────────────────
# The cc.ui.visual.* area library: markdown rendering, code highlighting,
# structured/file-edit diffs. Split out of the single cc_ui target so a body
# edit in this area recompiles only this area's objects (its own CXX.dd
# dyndep file), not the whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.visual.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Visual is UI9_RANK 1 (a pure leaf: zero cc.ui
# imports), so it links no other cc_ui_<area> library.
add_library(cc_ui_visual)
target_sources(cc_ui_visual
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/visual/code_highlight.cppm
        ui/visual/file_edit_tool_diff.cppm
        ui/visual/markdown.cppm
        ui/visual/structured_diff.cppm
)
# Module implementation units for cc.ui.visual.markdown (RFC 0001 Phase C
# batch 6): lexer / LRU cache / linkify / code-block (isolates the
# cc.ui.visual.code_highlight import closure) / block+inline renderers /
# public API + StreamingMarkdown / interactive component vtable anchor.
# Moved from cc_ui unchanged; they implement the cc.ui.visual.markdown
# module interface owned by this target.
target_sources(cc_ui_visual PRIVATE
    ui/visual/markdown_lexer_impl.cpp
    ui/visual/markdown_cache_impl.cpp
    ui/visual/markdown_linkify_impl.cpp
    ui/visual/markdown_render_code_impl.cpp
    ui/visual/markdown_render_impl.cpp
    ui/visual/markdown_api_impl.cpp
    ui/visual/markdown_component_impl.cpp
)
# External deps only: cc.types.types (code_highlight), cc.utils.file_edit /
# cc.utils.string_utils (structured_diff, file_edit_tool_diff), and FTXUI
# (DOM/component headers). cc_std's `import std;` BMI arrives via the
# directory-level link_libraries(cc_std). Over-linking is safe (and matches
# the previous cc_ui.cmake behaviour).
target_link_libraries(cc_ui_visual
    PUBLIC
        cc_types
        cc_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
