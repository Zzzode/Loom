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
        # RFC 0001 Phase D B5e: moved from cc_utils (src/utils/skills/).
        # loom_hints.cppm declares cc.skills.hints (was cc.utils.loom_code_hints);
        # skill_usage.cppm declares cc.skills.support (was cc.utils.skill_usage).
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
target_sources(cc_skills PRIVATE
    skills/skill_usage_impl.cpp
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
