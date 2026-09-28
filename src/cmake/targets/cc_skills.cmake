# ─── cc_skills: Skill System ─────────────────────────────────────────────────
add_library(cc_skills)
target_sources(cc_skills
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
        skills/lorem_ipsum.cppm
        skills/mcp_skill_builders.cppm
        skills/remember.cppm
        skills/simplify.cppm
        skills/verify_content.cppm
)
# RFC-0001 B15: the load_skills_dir cc_tools edge left with the lifted agent
# subtree (the skill executor now lives in cc_orchestration); no skills
# module imports cc.tools.* anymore.
target_link_libraries(cc_skills
    PUBLIC
        cc_utils
        cc_config           # skills/bundled/* import cc.config.config (was
                            # transitive via cc_tools PUBLIC before B15).
        cc_skills_core
        yyjson
)
