# ─── loom_ui_visual: UI visual rendering (RFC 0002 F4) ─────────────────────────
# The loom.ui.visual.* area library: markdown rendering, code highlighting,
# structured/file-edit diffs. Split out of the single loom_ui target so a body
# edit in this area recompiles only this area's objects (its own CXX.dd
# dyndep file), not the whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.visual.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Visual is UI9_RANK 1 (a pure leaf: zero loom.ui
# imports), so it links no other loom_ui_<area> library.
add_library(loom_ui_visual)
target_sources(loom_ui_visual
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/visual/code_highlight.cppm
        ui/visual/file_edit_tool_diff.cppm
        ui/visual/markdown.cppm
        ui/visual/structured_diff.cppm
)
# Module implementation units for loom.ui.visual.markdown (RFC 0001 Phase C
# batch 6): lexer / LRU cache / linkify / code-block (isolates the
# loom.ui.visual.code_highlight import closure) / block+inline renderers /
# public API + StreamingMarkdown / interactive component vtable anchor.
# Moved from loom_ui unchanged; they implement the loom.ui.visual.markdown
# module interface owned by this target.
#
# code_highlight_impl.cpp implements the loom.ui.visual.code_highlight module
# interface: the ColoredTextLine Node + colored_text_line() factory body
# (inline-def ratchet).
target_sources(loom_ui_visual PRIVATE
    ui/visual/code_highlight_impl.cpp
    ui/visual/markdown_lexer_impl.cpp
    ui/visual/markdown_cache_impl.cpp
    ui/visual/markdown_linkify_impl.cpp
    ui/visual/markdown_render_code_impl.cpp
    ui/visual/markdown_render_impl.cpp
    ui/visual/markdown_api_impl.cpp
    ui/visual/markdown_component_impl.cpp
    # HTML serializer for CommonMark/GFM conformance testing.
    ui/visual/markdown_html_impl.cpp
)
# External deps only: loom.types.types (code_highlight), loom.fs.edit.file_edit /
# loom.text.string_utils (structured_diff, file_edit_tool_diff), and FTXUI
# (DOM/component headers). loom_std's `import std;` BMI arrives via the
# directory-level link_libraries(loom_std). Over-linking is safe (and matches
# the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_visual
    PUBLIC
        loom_types
        loom_utils
        ftxui::screen
        ftxui::dom
        ftxui::component
)
