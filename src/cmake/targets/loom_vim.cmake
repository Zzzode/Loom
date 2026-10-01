# ─── loom_vim: Vim Mode ─────────────────────────────────────────────────────────
add_library(loom_vim)
target_sources(loom_vim
    PUBLIC FILE_SET CXX_MODULES FILES
        vim/vim_types.cppm
        vim/vim_controller.cppm
        vim/vim_mode.cppm
)
target_link_libraries(loom_vim PUBLIC loom_utils)
