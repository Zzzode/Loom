// C++23 Module: runtime backend installation for the orchestration layer.
//
// RFC 0001 Phase B: cc_orchestration is the rank-9 layer that wires concrete
// lower-ranked services (cc_services, cc_skills, ...) into the callback
// ports owned by the tools layer. This module installs:
//   - the cc.services.image-backed image codec (cc.tools.image_codec.port,
//     B11);
//   - the cc.skills::SkillLoader-backed skill executor
//     (cc.tools.runtime_backends.port, B12).
// Later batches add the lifted runtime tool backends in this same module.
module;

#include <cstdint>
#include <cstdlib>  // std::getenv("HOME") in the skill executor

export module cc.orchestration.runtime_backends;

import std;

import cc.services.image;
import cc.tools.image_codec.port;
import cc.tools.runtime_backends.port;
import cc.types.types;
import cc.types.tool_types;
// Core DTOs come from the rank-1 contract leaves directly (cc.tools.tool
// only re-exports tool_types; the moved skill block names none of its own
// symbols, so importing it here would be a dead edge. B15's register code
// adds cc.tools.tool when it genuinely uses ToolRegistry/ITool.
import cc.skills.skill;
import cc.tools.agent_runtime;
import cc.tools.runtime_registry;

export namespace cc::orchestration {

namespace fs = std::filesystem;

/// Build the production image codec: a thin forwarder over
/// cc::services::image::ImageService. Exposed (not just used by the
/// installer) so test fixtures can reinstall the real codec after clearing
/// the process-global slot.
[[nodiscard]] inline cc::tools::image_codec::Codec make_image_codec() {
    namespace image_codec = cc::tools::image_codec;
    namespace svc_image = cc::services::image;

    image_codec::Codec result;

    result.get_info = [](const std::filesystem::path& path)
        -> std::expected<image_codec::ImageInfo, std::string> {
        auto info = svc_image::ImageService::get_info(path);
        if (!info) {
            return std::unexpected(info.error().message);
        }
        return image_codec::ImageInfo{
            .width = info->width,
            .height = info->height,
            .size_bytes = info->size_bytes,
            .mime = std::string(svc_image::format_to_mime(info->format)),
            .summary = info->summary(),
        };
    };

    result.to_base64 = [](std::span<const std::uint8_t> data) -> std::string {
        return svc_image::ImageService::to_base64(data);
    };

    result.from_base64 = [](std::string_view encoded)
        -> std::expected<std::vector<std::uint8_t>, std::string> {
        auto decoded = svc_image::ImageService::from_base64(encoded);
        if (!decoded) {
            return std::unexpected(decoded.error().message);
        }
        return std::move(*decoded);
    };

    return result;
}

/// Build the production SkillLoader executor: directory/plugin skill
/// discovery lifted verbatim out of cc.tools.runtime_registry's skill branch
/// (HOME/.codex/skills, HOME/.agents/skills, cwd/skills, plus plugin
/// component skills via agent_runtime). Returns std::nullopt when no skill
/// matches so the tools-side terminal manual SKILL.md walk runs. Exposed (not
/// just used by the installer) so test fixtures can reinstall it after
/// clearing the process-global slot.
///
/// HOME/cwd/plugin path resolution happens PER CALL inside the lambda: tests
/// (and the server) change the working directory at runtime, so evaluating
/// these paths when the executor is built would freeze the wrong roots.
[[nodiscard]] inline cc::tools::SkillLoaderExecutor make_skill_loader_executor() {
    return [](const cc::core::ToolInput& input)
        -> std::optional<cc::core::Result<cc::core::ToolResult>> {
        auto name = cc::tools::detail::json_string(input.json(), "name")
            .or_else([&] { return cc::tools::detail::json_string(input.json(), "skill"); });
        if (!name || name->empty()) return std::nullopt;

        cc::skills::SkillLoader loader;
        if (const char* home = std::getenv("HOME")) {
            loader.add_search_path(fs::path{home} / ".codex" / "skills");
            loader.add_search_path(fs::path{home} / ".agents" / "skills");
        }
        loader.add_search_path(fs::current_path() / "skills");

        std::vector<std::pair<std::string, fs::path>> plugin_skill_paths;
        for (const auto& plugin : cc::tools::agent_runtime::discover_plugin_component_paths()) {
            for (const auto& path : plugin.skills_paths) {
                plugin_skill_paths.emplace_back(plugin.plugin_name, path);
            }
        }
        auto discovered = loader.discover_all_with_plugin_skills(plugin_skill_paths);
        if (discovered) {
            for (const auto& skill : *discovered) {
                if (skill.name == *name) return cc::core::ToolResult::success(skill.content);
            }
        }

        return std::nullopt;
    };
}

/// Install every orchestration-owned runtime backend exactly once. The
/// std::call_once guard makes the call safe from the per-request server
/// route threads; repeat calls after the first are no-ops. This batch wires
/// the image codec and the SkillLoader skill executor — the lifted runtime
/// tool backends arrive in later Phase B batches.
inline void install_runtime_backends() {
    static std::once_flag once;
    std::call_once(once, [] {
        cc::tools::image_codec::set_codec(make_image_codec());
        cc::tools::set_skill_loader_executor(make_skill_loader_executor());
    });
}

} // namespace cc::orchestration
