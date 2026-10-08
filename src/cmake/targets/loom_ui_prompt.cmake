# ─── loom_ui_prompt: UI prompt input (RFC 0002 F4) ────────────────────────────
# The cc.ui.prompt.* area library: the prompt input box, autocomplete sources,
# at-attachment resolution, fuzzy ranking, mode indicator, placeholder
# cascade, highlights, paste handling, vim input, and the stash notice.
# Split out of the single loom_ui target so a body edit in this area recompiles
# only this area's objects (its own CXX.dd dyndep file), not the whole loom_ui
# closure.
#
# Grouped by MODULE-NAME area (export module cc.ui.prompt.*), not by
# directory — name/path decoupling means the grouping rule is stated, not
# inferred from the tree. Prompt is UI9_RANK 4: it may link lower-ranked
# areas (foundation rank 2, chrome rank 3) and never a higher-ranked one.
add_library(loom_ui_prompt)
target_sources(loom_ui_prompt
    PUBLIC FILE_SET CXX_MODULES FILES
        ui/prompt/at_attachments.cppm
        ui/prompt/autocomplete_sources.cppm
        ui/prompt/combined_highlights.cppm
        ui/prompt/file_index.cppm
        ui/prompt/fuzzy_rank_nucleo.cppm
        ui/prompt/mode_indicator.cppm
        ui/prompt/placeholder_cascade.cppm
        ui/prompt/prompt_input.cppm
        ui/prompt/prompt_input_footer.cppm
        ui/prompt/prompt_paste_handler.cppm
        ui/prompt/prompt_stash_notice.cppm
        ui/prompt/vim_input.cppm
)
# Module implementation units for cc.ui.prompt.at_attachments and
# cc.ui.prompt.autocomplete_sources (RFC-0001 B15): the at-attachment /
# autocomplete bodies, extracted verbatim from their interfaces. Bodies in
# the impl units keep the declarations-only BMIs cheap and give fan-out = 1
# on a body edit. Moved from loom_ui unchanged; they implement the module
# interfaces owned by this target.
target_sources(loom_ui_prompt PRIVATE
    ui/prompt/at_attachments_impl.cpp
    ui/prompt/autocomplete_sources_impl.cpp
)
# cc.ui.foundation.* (design_tokens / design_figures / theme_provider /
# ui_types) and cc.ui.chrome.* (ansi_render / layout). External deps:
# cc.types.types, cc.utils.* (clipboard / json / parse_references /
# text_highlighting / bash_execution), cc.tools.agent_runtime,
# cc.orchestration.tools.mcp (the B15 at_attachments/autocomplete impl TUs),
# cc.skills.* (autocomplete sources), cc.vim.vim_controller, and FTXUI
# (component / dom / screen headers). loom_std's `import std;` BMI arrives via
# the directory-level link_libraries(loom_std). Over-linking is safe (and
# matches the previous loom_ui.cmake behaviour).
target_link_libraries(loom_ui_prompt
    PUBLIC
        loom_ui_foundation
        loom_ui_chrome
        loom_types
        loom_utils
        loom_tools
        loom_orchestration
        loom_skills
        loom_vim
        ftxui::screen
        ftxui::dom
        ftxui::component
)
