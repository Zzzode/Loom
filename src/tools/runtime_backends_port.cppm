/// @file runtime_backends_port.cppm
/// @brief Unified runtime-backends seam — orchestration-owned executors.
///
/// Tool dispatch needs concrete backends that live above the tools layer
/// (cc.skills discovery, lifted runtime tools, MCP/LSP/agent services), but
/// cc_tools cannot depend on cc_orchestration (rank 8 -> rank 9 would be an
/// upward edge). The concrete backends are installed once at process startup
/// via cc::orchestration::install_runtime_backends() and acquired through
/// accessors declared in cc.tools.runtime_registry.
///
/// This module is THE unified registry seam (one module, one installer):
/// B12 contributes only the SkillLoader executor type; later Phase B batches
/// extend this same module with the runtime tool executors, MCP providers,
/// the missing-tool handler and the agent factory so the service-backed tool
/// sinks never diverge.
module;

export module cc.tools.runtime_backends.port;

import std;

import cc.types.types;
import cc.types.tool_types;

export namespace cc::tools {

/// Backs the runtime 'skill' tool's directory/plugin discovery. Receives the
/// raw ToolInput and returns the tool Result when it resolved a skill, or
/// std::nullopt to let the tools-side terminal manual SKILL.md walk run. The
/// concrete cc::skills::SkillLoader-backed implementation is installed by
/// cc_orchestration; binaries without a deployment (and tests that do not
/// install the services guard) run with no executor attached.
using SkillLoaderExecutor = std::function<
    std::optional<cc::core::Result<cc::core::ToolResult>>(
        const cc::core::ToolInput&)>;

} // namespace cc::tools
