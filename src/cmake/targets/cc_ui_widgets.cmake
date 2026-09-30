# ─── cc_ui_widgets: UI widgets (RFC 0002 F4) ───────────────────────────────
# The cc.ui.widgets.* area library: reusable FTXUI controls — the component
# umbrella (all_components), the shared component helpers, custom select,
# dev bar, fast icon, passes, PR badge, spinner (+ animations), stats,
# tag tabs, and the text input (interface + buffer/events/vim/render impl
# units). Split out of the single cc_ui target so a body edit in this area
# recompiles only this area's objects (its own CXX.dd dyndep file), not the
# whole cc_ui closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.widgets.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Widgets is UI9_RANK 5: it may link lower-ranked
# areas (visual/tools rank 1, foundation rank 2, chrome rank 3, prompt
# rank 4) and never a higher-ranked one.
add_library(cc_ui_widgets)
target_sources(cc_ui_widgets
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
# Module implementation units for cc.ui.widgets.text_input
# (RFC 0001 Phase C batch 8): editing/history/paste core, readline
# event dispatch + reverse search, vim dispatch/operators, and the
# FTXUI renderers + TextInput() factory. Bodies in the impl units keep
# the declarations-only BMI cheap and give fan-out = 1 on a body edit.
# Moved from cc_ui unchanged; they implement the module interface owned
# by this target.
target_sources(cc_ui_widgets PRIVATE
    ui/widgets/text_input_buffer.cpp
    ui/widgets/text_input_events.cpp
    ui/widgets/text_input_vim.cpp
    ui/widgets/text_input_render.cpp
)
# cc.ui.foundation.* (components_figures / component_primitives /
# design_tokens / design_figures / theme_provider / ui_types),
# cc.ui.visual.markdown (components), and cc.ui.prompt.* (text_input +
# text_input_widget: prompt_paste_handler / placeholder_cascade /
# combined_highlights). External deps: cc.types.types, cc.utils.*
# (parse_references / text_highlighting), cc.vim.vim_controller, and
# FTXUI (component / dom / screen headers). cc_std's `import std;` BMI
# arrives via the directory-level link_libraries(cc_std). Over-linking
# is safe (and matches the previous cc_ui.cmake behaviour).
target_link_libraries(cc_ui_widgets
    PUBLIC
        cc_ui_foundation
        cc_ui_visual
        cc_ui_prompt
        cc_types
        cc_utils
        cc_vim
        ftxui::screen
        ftxui::dom
        ftxui::component
)
