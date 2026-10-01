// app_skills_menu.cpp — plain impl unit for cc.ui.app.app. Owns the
// skills-menu helper bodies (RFC 0001 Phase C batch 1):
//   AppAdapter static members: skill_source_order,
//     is_visible_skills_menu_source, skills_menu_token_estimate,
//     collapse_home_path, skill_source_group_title, FormatSkillsMenuOutput
//   AppAdapter member: OpenSkillsMenu
//
// Declarations stay in app.cppm. OpenSkillsMenu mutates screen_state_ and
// calls PostRenderEvent(), whose body moved to app_animation.cpp in batch
// 2 (a private member callable from member functions in any impl unit).
//
// LLVM #184957: like app_extra_methods.cpp / app_handle_submit.cpp /
// app_prompt_suggestion_wiring.cpp / app_team.cpp / app_run.cpp, this unit
// must NOT `import std;` — under the reduced-BMI writer a cold module cache
// mis-merges the global aligned operator new when an app impl unit imports
// std while the primary's GMF pulls libc++ textually via FTXUI. Keep textual
// std headers in the global module fragment (see CMakeLists.txt:283-290).
module;

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module loom.ui.app.app;

import loom.ui.prompt.autocomplete_sources;
import loom.ui.screens.repl_state;

namespace loom::ui {

namespace acsrc = loom::ui::autocomplete_sources;

// ============================================================
// Skills-menu helpers (AppAdapter static members)
// ============================================================

[[nodiscard]] int AppAdapter::skill_source_order(std::string_view source) {
    if (source == "project") return 0;
    if (source == "user") return 1;
    if (source == "plugin") return 2;
    if (source == "mcp") return 3;
    return 4;
}

[[nodiscard]] bool AppAdapter::is_visible_skills_menu_source(
    std::string_view source) {
    return source == "project" ||
           source == "user" ||
           source == "plugin" ||
           source == "mcp";
}

[[nodiscard]] std::size_t AppAdapter::skills_menu_token_estimate(
    const acsrc::SkillSuggestionData& skill) {
    std::string frontmatter = skill.name;
    if (!skill.description.empty()) {
        frontmatter.push_back(' ');
        frontmatter += skill.description;
    }
    return rough_js_token_count(frontmatter);
}

[[nodiscard]] std::string AppAdapter::collapse_home_path(std::string path) {
    if (const char* home = std::getenv("HOME"); home && *home) {
        const std::string home_path(home);
        if (path == home_path) return "~";
        if (path.starts_with(home_path + "/")) {
            return "~" + path.substr(home_path.size());
        }
    }
    return path;
}

[[nodiscard]] std::string AppAdapter::skill_source_group_title(
    const acsrc::SkillSuggestionData& skill) {
    if (skill.source == "project") {
        return skill.source_detail.empty()
            ? "Project skills"
            : "Project skills (" + collapse_home_path(skill.source_detail) + ")";
    }
    if (skill.source == "user") return "User skills (~/.loom/skills)";
    if (skill.source == "plugin") {
        return skill.source_detail.empty()
            ? "Plugin skills"
            : "Plugin skills (" + skill.source_detail + ")";
    }
    if (skill.source == "mcp") return "MCP skills";
    return "Other skills";
}

[[nodiscard]] std::string AppAdapter::FormatSkillsMenuOutput(
    std::vector<acsrc::SkillSuggestionData> skills) {
    std::erase_if(skills, [](const auto& skill) {
        return !is_visible_skills_menu_source(skill.source);
    });

    std::ranges::sort(skills, [](const auto& a, const auto& b) {
        const int ao = skill_source_order(a.source);
        const int bo = skill_source_order(b.source);
        if (ao != bo) return ao < bo;
        if (a.source_detail != b.source_detail) {
            return a.source_detail < b.source_detail;
        }
        return a.name < b.name;
    });

    std::string out;
    out += "Skills\n";
    out += std::format(
        "{} skill{}\n",
        skills.size(),
        skills.size() == 1 ? "" : "s");

    if (skills.empty()) {
        out += "\nNo skills found.\n";
        out += "Create skills under `.loom/skills` or `~/.loom/skills`.\n";
        return out;
    }

    std::string current_group;
    bool first_group = true;
    for (const auto& skill : skills) {
        const std::string group = skill_source_group_title(skill);
        if (group != current_group) {
            if (!first_group) out += "\n";
            first_group = false;
            current_group = group;
            out += "\n" + current_group + "\n";
        }

        out += skill.name;
        out += std::format(
            " · ~{} description tokens",
            skills_menu_token_estimate(skill));
        out += "\n";
    }
    return out;
}

void AppAdapter::OpenSkillsMenu() {
    const auto& skills = cached_skills_;
    screen_state_->mode = repl::ReplMode::Normal;
    screen_state_->active_local_jsx_command = true;
    screen_state_->active_local_jsx_command_name = "skills";
    screen_state_->active_local_jsx_command_args.clear();
    screen_state_->active_local_jsx_content =
        FormatSkillsMenuOutput(std::move(skills));
    screen_state_->messages_store.scroll_offset = 0;
    screen_state_->messages_store.scroll_pinned_to_bottom = false;
    PostRenderEvent();
}

} // namespace loom::ui
