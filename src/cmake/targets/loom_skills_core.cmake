# ─── loom_skills_core: Skill primitives without tool dependencies ──────────────
add_library(loom_skills_core)
target_sources(loom_skills_core
    PUBLIC FILE_SET CXX_MODULES FILES
        skills/skill.cppm
        skills/file_access_port.cppm
)
target_link_libraries(loom_skills_core
    PUBLIC
        loom_types
)
