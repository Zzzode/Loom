// C++23 Module: runtime backend installation for the orchestration layer.
//
// RFC 0001 Phase B: cc_orchestration is the rank-9 layer that wires concrete
// lower-ranked services (cc_services, cc_skills, ...) into the callback
// ports owned by the tools layer. This is the ONE installer:
//   - cc.services.image-backed image codec (cc.tools.image_codec.port, B11);
//   - cc.skills::SkillLoader-backed skill executor (B12);
//   - the six lifted runtime tool backends (lsp / mcp / list_mcp_resources /
//     read_mcp_resource / mcp_auth / computer_use, B15);
//   - the Agent tool factory (cc.orchestration.agent, B15);
//   - the MCP-connectivity -> hook snapshot sink (rehomed bridge, B15).
//
// The registry missing-tool fallback (make_missing_tool_backend) and the MCP
// snapshot-derived providers (collect_mcp_*) are BUILT/DECLARED here but not
// installed into slots: roots bind them directly onto each ToolRegistry and
// request config. The post-B15 process-global slots had no readers.
//
// All bodies that need services types live in the implementation units
// runtime_backends_{lsp,mcp,computer_use}.cpp; the install entry point is
// std::call_once-guarded so the per-connection server threads can call it
// safely (repeat calls are no-ops).
module;

#include <cstdint>
#include <cstdlib>  // std::getenv("HOME") in the skill executor

export module loom.orchestration.runtime_backends;

import std;

import loom.services.image;
import loom.tools.image_codec.port;
import loom.tools.runtime_backends.port;
import loom.types.types;
import loom.types.tool_types;
import loom.tools.tool;
import loom.tools.runtime_registry;
import loom.skills.skill;
import loom.tools.agent_runtime;
import loom.orchestration.tools.lsp;
import loom.orchestration.tools.mcp;
import loom.orchestration.agent;
import loom.orchestration.mcp_connectivity;

// Inter-TU declarations: visible to every implementation unit of THIS module
// but NOT exported (importers reach these only through the installed slots).
namespace loom::orchestration::detail {

using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

[[nodiscard]] Result<ToolResult> lsp_backend(const ToolInput& input);
[[nodiscard]] Result<ToolResult> mcp_backend(const ToolInput& input);
[[nodiscard]] Result<ToolResult> list_mcp_resources_backend(const ToolInput& input);
[[nodiscard]] Result<ToolResult> read_mcp_resource_backend(const ToolInput& input);
[[nodiscard]] Result<ToolResult> mcp_auth_backend(const ToolInput& input);
// c13b structured config runtime tool.
[[nodiscard]] Result<ToolResult> config_backend(const ToolInput& input);

} // namespace loom::orchestration::detail

// The moved computer-use implementation unit keeps namespace loom::tools (the
// zero-call-site-churn policy of the agent/mcp lifts).
namespace loom::tools::detail {

using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

[[nodiscard]] Result<ToolResult> execute_computer_use(const ToolInput& input);

} // namespace loom::tools::detail

export namespace loom::tools {

// MCP tool-pool collectors, lifted verbatim from
// runtime_registry_register.cpp (bodies in runtime_backends_mcp.cpp). They
// stay in namespace loom::tools so main.cpp / server_routes call sites are
// unchanged; their home module is now cc.orchestration.runtime_backends.
[[nodiscard]] std::vector<loom::core::ToolDefinition> collect_mcp_tool_definitions();

[[nodiscard]] std::unordered_map<std::string, std::string>
collect_mcp_input_schemas();

} // export namespace loom::tools

export namespace loom::orchestration {

namespace fs = std::filesystem;

/// Build the production image codec: a thin forwarder over
/// loom::services::image::ImageService. Exposed (not just used by the
/// installer) so test fixtures can reinstall the real codec after clearing
/// the process-global slot.
[[nodiscard]] inline loom::tools::image_codec::Codec make_image_codec() {
    namespace image_codec = loom::tools::image_codec;
    namespace svc_image = loom::services::image;

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
[[nodiscard]] inline loom::tools::SkillLoaderExecutor make_skill_loader_executor() {
    return [](const loom::core::ToolInput& input)
        -> std::optional<loom::core::Result<loom::core::ToolResult>> {
        auto name = loom::tools::detail::json_string(input.json(), "name")
            .or_else([&] { return loom::tools::detail::json_string(input.json(), "skill"); });
        if (!name || name->empty()) return std::nullopt;

        loom::skills::SkillLoader loader;
        if (const char* home = std::getenv("HOME")) {
            loader.add_search_path(fs::path{home} / ".codex" / "skills");
            loader.add_search_path(fs::path{home} / ".agents" / "skills");
        }
        loader.add_search_path(fs::current_path() / "skills");

        std::vector<std::pair<std::string, fs::path>> plugin_skill_paths;
        for (const auto& plugin : loom::tools::agent_runtime::discover_plugin_component_paths()) {
            for (const auto& path : plugin.skills_paths) {
                plugin_skill_paths.emplace_back(plugin.plugin_name, path);
            }
        }
        auto discovered = loader.discover_all_with_plugin_skills(plugin_skill_paths);
        if (discovered) {
            for (const auto& skill : *discovered) {
                if (skill.name == *name) return loom::core::ToolResult::success(skill.content);
            }
        }

        return std::nullopt;
    };
}

/// Build the c13b structured config runtime tool backend. Exposed (not just
/// used by the installer) so the null-slot test can reinstall it after
/// clearing the process-global slot. Path/env resolution and the quiet load
/// happen per call inside detail::config_backend.
[[nodiscard]] inline loom::tools::RuntimeToolExecutor make_config_backend() {
    return [](const loom::core::ToolInput& input) {
        return detail::config_backend(input);
    };
}

/// Build the registry missing-tool fallback: route unregistered tool names
/// (per-server MCP tools exposed as short names) to connected MCP servers in
/// all_statuses() order. Lifted verbatim from the main.cpp / server_routes
/// lambdas (including std::string{input.json()}, last_error capture, and the
/// exact ToolNotFound text); the last tried server's error, including a
/// terminal ToolNotFound, is appended to the final message verbatim.
[[nodiscard]] inline loom::core::ToolRegistry::MissingToolHandler
make_missing_tool_backend() {
    return [](std::string_view tool_name,
              const loom::core::ToolInput& input) -> loom::core::Result<loom::core::ToolResult> {
        auto& runtime = loom::tools::NativeMcpRuntime::instance();
        std::string last_error;
        // Connect servers whose status we know (from all_statuses), then add
        // any others — NativeMcpRuntime::call_tool will auto-connect them.
        auto statuses = runtime.all_statuses();
        for (const auto& s : statuses) {
            auto result = runtime.call_tool(s.name, tool_name, std::string{input.json()});
            if (result) {
                return loom::tools::mcp_result_to_tool_result(*result);
            }
            // Byte-exact with the three pre-B15 lambda sites
            // (run_runtime_tool_once, the interactive main path,
            // server_routes): the last server's error is kept
            // unconditionally — including a terminal ToolNotFound.
            last_error = std::string{loom::tools::format_error(result.error())};
        }
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::ToolNotFound,
            std::format("Tool '{}' not found in registry or on any "
                        "configured MCP server{}",
                        tool_name,
                        last_error.empty() ? "" : " (" + last_error + ")")));
    };
}

/// Install every orchestration-owned runtime backend exactly once. The
/// std::call_once guard makes the call safe from the per-request server
/// route threads; repeat calls after the first are no-ops. Call at process
/// startup (main) and before the accept loop / per-session registry in the
/// server — never from inside a request handler mid-flight.
inline void install_runtime_backends() {
    static std::once_flag once;
    std::call_once(once, [] {
        loom::tools::image_codec::set_codec(make_image_codec());
        loom::tools::set_skill_loader_executor(make_skill_loader_executor());

        loom::tools::set_lsp_backend(&detail::lsp_backend);
        loom::tools::set_mcp_backend(&detail::mcp_backend);
        loom::tools::set_list_mcp_resources_backend(&detail::list_mcp_resources_backend);
        loom::tools::set_read_mcp_resource_backend(&detail::read_mcp_resource_backend);
        loom::tools::set_mcp_auth_backend(&detail::mcp_auth_backend);
        // ONE slot covers both 'computer_use' and the native-wire 'computer';
        // the dispatcher funnels both names here.
        loom::tools::set_computer_use_backend(
            [](const loom::core::ToolInput& input) {
                return loom::tools::detail::execute_computer_use(input);
            });
        // c13b: structured user-tier config tool.
        loom::tools::set_config_backend(make_config_backend());

        // Agent tool factory (exact 5-arg make_agent_tool shape). The factory
        // is invoked per register_runtime_tools() call with that call's own
        // permission checker copy — install-time capture would freeze one
        // registry's checker.
        loom::tools::set_agent_tool_factory(
            [](loom::tools::AgentConfig config,
               int depth,
               loom::core::ToolRegistry* registry,
               loom::tools::AgentLivePermissionCheckFn permission_check,
               bool permission_hook_valid_for_background) {
                return loom::tools::make_agent_tool(
                    std::move(config),
                    depth,
                    registry,
                    std::move(permission_check),
                    permission_hook_valid_for_background);
            });

        // MCP connectivity -> hook snapshot sink (rehomed from cc_bootstrap).
        loom::orchestration::mcp_connectivity::wire_mcp_connectivity();
    });
}

} // namespace loom::orchestration
