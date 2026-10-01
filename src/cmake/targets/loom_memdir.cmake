# ─── loom_memdir: Memory Directory ──────────────────────────────────────────────
add_library(loom_memdir)
target_sources(loom_memdir
    PUBLIC FILE_SET CXX_MODULES FILES
        memdir/memdir.cppm
        memdir/paths.cppm
)
target_link_libraries(loom_memdir PUBLIC loom_utils loom_constants)
