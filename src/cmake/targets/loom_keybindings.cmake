# ─── loom_keybindings: Keybinding System ────────────────────────────────────────
add_library(loom_keybindings)
target_sources(loom_keybindings
    PUBLIC FILE_SET CXX_MODULES FILES
        keybindings/defaults.cppm
        keybindings/load_user_bindings.cppm
        keybindings/schema.cppm
        keybindings/shortcut_format.cppm
        keybindings/template.cppm
        keybindings/validate.cppm
)
target_link_libraries(loom_keybindings PUBLIC loom_utils loom_config)
