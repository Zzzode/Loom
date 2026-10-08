# ─── loom_ui_widgets: UI widgets (RFC 0002 F4) ───────────────────────────────
# The loom.ui.widgets.* area library: reusable FTXUI controls — the component
# umbrella (all_components), the shared component helpers, custom select,
# dev bar, fast icon, passes, PR badge, spinner (+ animations), stats,
# tag tabs, and the text input (interface + buffer/events/vim/render impl
# units). Split out of the single loom_ui target so a body edit in this area
# recompiles only this area's objects (its own CXX.dd dyndep file), not the
# whole loom_ui closure.
#
# Grouped by MODULE-NAME area (export module loom.ui.widgets.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Widgets is UI9_RANK 5: it may link lower-ranked
# areas (visual/tools rank 1, foundation rank 2, chrome rank 3, prompt
# rank 4) and never a higher-ranked one.
add_library(loom_ui_widgets)
target_sources(loom_ui_widgets
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/widgets/all_components.cppm
        ui/widgets/components.cppm
        ui/widgets/custom_select.cppm
        ui/widgets/dev_bar.cppm
        ui/widgets/fast_icon.cppm
        ui/widgets/passes.cppm
        ui/widgets/pr_badge.cppm
        ui/widgets/spinner.cppm
        ui/widgets/spinner_animations.cppm
        ui/widgets/stats.cppm
        ui/widgets/tag_tabs.cppm
        ui/widgets/text_input.cppm
        ui/widgets/text_input_widget.cppm
)
# Module implementation units for loom.ui.widgets.text_input
# (RFC 0001 Phase C batch 8): editing/history/paste core, readline
# event dispatch + reverse search, vim dispatch/operators, and the
# FTXUI renderers + TextInput() factory. Bodies in the impl units keep
# the declarations-only BMI cheap and give fan-out = 1 on a body edit.
# Moved from loom_ui unchanged; they implement the module interface owned
# by this target.
target_sources(loom_ui_widgets PRIVATE
    ui/widgets/text_input_buffer.cpp
    ui/widgets/text_input_events.cpp
    ui/widgets/text_input_vim.cpp
    ui/widgets/text_input_render.cpp
)
# loom.ui.foundation.* (component_primitives /
# design_tokens / design_figures / theme_provider / ui_types),
# loom.ui.visual.markdown (components), and loom.ui.prompt.* (text_input +
# text_input_widget: prompt_paste_handler / placeholder_cascade /
# combined_highlights). External deps: loom.types.types, loom.utils.*
# (parse_references / text_highlighting), loom.vim.vim_controller, and
# FTXUI (component / dom / screen headers). loom_std's `import std;` BMI
# arrives via the directory-level link_libraries(loom_std). Over-linking
# is safe (and matches the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_widgets
    PUBLIC
        loom_ui_foundation
        loom_ui_visual
        loom_ui_prompt
        loom_types
        loom_utils
        loom_vim
        ftxui::screen
        ftxui::dom
        ftxui::component
)
