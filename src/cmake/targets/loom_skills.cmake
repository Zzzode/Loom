# ─── loom_skills: Skill System ─────────────────────────────────────────────────
add_library(loom_skills)
target_sources(loom_skills
    PUBLIC FILE_SET CXX_MODULES FILES
        skills/bundled.cppm
        skills/bundled/loom_in_chrome.cppm
        skills/bundled/debug.cppm
        skills/bundled/loop.cppm
        skills/bundled/skill_keybindings.cppm
        skills/bundled/skillify.cppm
        skills/bundled/stuck.cppm
        skills/bundled/update_config.cppm
        skills/loom_api.cppm
        skills/loom_api_content.cppm
        skills/keybindings.cppm
        skills/load_skills_dir.cppm
        # RFC 0001 Phase D B5e: moved from loom_utils (src/utils/skills/).
        # loom_hints.cppm declares loom.skills.hints (was loom.utils.loom_code_hints);
        # skill_usage.cppm declares loom.skills.support (was loom.utils.skill_usage).
        skills/loom_hints.cppm
        skills/lorem_ipsum.cppm
        skills/mcp_skill_builders.cppm
        skills/remember.cppm
        skills/simplify.cppm
        skills/skill_usage.cppm
        skills/verify_content.cppm
)
# RFC 0001 Phase D B5e: skill_usage impl unit — heavy I/O (sidecar load/write,
# debounce map) kept PRIVATE, mirroring the utils/serdes/json_impl.cpp pattern.
target_sources(loom_skills PRIVATE
    skills/skill_usage_impl.cpp
)
# RFC-0001 B15: the load_skills_dir loom_tools edge left with the lifted agent
# subtree (the skill executor now lives in loom_orchestration); no skills
# module imports loom.tools.* anymore.
target_link_libraries(loom_skills
    PUBLIC
        loom_utils
        loom_config           # skills/bundled/* import loom.config.config (was
                            # transitive via loom_tools PUBLIC before B15).
        loom_skills_core
        yyjson
)
