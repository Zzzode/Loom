/// @file runtime_backends_port.cppm
/// @brief Unified runtime-backends seam — orchestration-owned executors.
///
/// Tool dispatch needs concrete backends that live above the tools layer
/// (lifted LSP/MCP/computer-use tools, the Agent tool, MCP connectivity), but
/// cc_tools cannot depend on cc_orchestration (rank 8 -> rank 9 would be an
/// upward edge). The concrete backends are installed once at process startup
/// via loom::orchestration::install_runtime_backends() and acquired through the
/// accessors declared below.
///
/// RFC-0001 B15 final shape — ONE seam, ONE installer. Every slot is a
/// std::function held in a single function-local RuntimeBackendSlots struct
/// anchored in the runtime_backends_port.cpp implementation unit:
///
///   - skill_loader   (B12): HOME/cwd/plugin SkillLoader discovery;
///   - six runtime tool backends: lsp, mcp, list_mcp_resources,
///     read_mcp_resource, mcp_auth, computer_use (the last one covers BOTH
///     the internal 'computer_use' and the native-wire 'computer' name);
///   - the Agent tool factory (5-arg make_agent_tool shape).
///
/// The registry missing-tool fallback and the MCP snapshot-derived tool
/// providers deliberately have NO slot here: roots (main / server_routes)
/// bind loom::orchestration::make_missing_tool_backend() and the
/// loom::tools::collect_mcp_* collectors directly onto each ToolRegistry /
/// request config, so a process-global slot had no reader.
///
/// The MCP *snapshot sink* itself (set_mcp_snapshots_sink) deliberately does
/// NOT live here: its signature names loom::services::mcp::McpServerSnapshot,
/// and naming a services type from this rank-8 port would recreate a
/// tools->services area edge (the post-B15 target graph has zero). The sink
/// stays with the lifted cc.orchestration.tools.mcp module, where importing
/// cc.services.mcp.* is a rank-9 -> rank-7 downward edge. No raw pointer or
/// type-erased snapshot ever crosses this seam.
module;

export module loom.tools.runtime_backends.port;

import std;

import loom.types.types;
import loom.types.tool_types;
// The surviving AgentToolFactory alias names loom::core::ITool and
// loom::core::ToolRegistry, both `class` declarations in cc.tools.tool that
// graph_check's textual symbol harvest does not see; the pre-C3
// ToolDefinition evidence left with the deleted MCP provider alias.
import loom.tools.tool;               // arch-check: keep-import
import loom.tools.agent_types;

export namespace loom::tools {

/// Backs the runtime 'skill' tool's directory/plugin discovery. Receives the
/// raw ToolInput and returns the tool Result when it resolved a skill, or
/// std::nullopt to let the tools-side terminal manual SKILL.md walk run.
using SkillLoaderExecutor = std::function<
    std::optional<loom::core::Result<loom::core::ToolResult>>(
        const loom::core::ToolInput&)>;

/// Shape of every lifted runtime tool backend (the LSP/MCP/computer-use
/// sextet). Nullable: an empty std::function means "no deployment installed
/// one" and dispatch returns the terminal no-runtime-handler error.
using RuntimeToolExecutor =
    std::function<loom::core::Result<loom::core::ToolResult>(
        const loom::core::ToolInput&)>;

/// 5-arg Agent tool factory — the exact make_agent_tool shape owned by the
/// lifted cc.orchestration.agent module:
///   (AgentConfig, depth, ToolRegistry*, live permission checker,
///    background-hook-valid flag) -> owned ITool.
using AgentToolFactory = std::function<std::unique_ptr<loom::core::ITool>(
    AgentConfig,
    int depth,
    loom::core::ToolRegistry* registry,
    AgentLivePermissionCheckFn permission_check,
    bool permission_hook_valid_for_background)>;

// ── SkillLoader executor (B12; storage moved into the unified slots B15) ──
void set_skill_loader_executor(SkillLoaderExecutor executor);
void clear_skill_loader_executor();
[[nodiscard]] std::optional<SkillLoaderExecutor>& skill_loader_executor_slot();

// ── The six lifted runtime tool backends ──────────────────────────────────
void set_lsp_backend(RuntimeToolExecutor executor);
void clear_lsp_backend();
[[nodiscard]] RuntimeToolExecutor& lsp_backend();

void set_mcp_backend(RuntimeToolExecutor executor);
void clear_mcp_backend();
[[nodiscard]] RuntimeToolExecutor& mcp_backend();

void set_list_mcp_resources_backend(RuntimeToolExecutor executor);
void clear_list_mcp_resources_backend();
[[nodiscard]] RuntimeToolExecutor& list_mcp_resources_backend();

void set_read_mcp_resource_backend(RuntimeToolExecutor executor);
void clear_read_mcp_resource_backend();
[[nodiscard]] RuntimeToolExecutor& read_mcp_resource_backend();

void set_mcp_auth_backend(RuntimeToolExecutor executor);
void clear_mcp_auth_backend();
[[nodiscard]] RuntimeToolExecutor& mcp_auth_backend();

/// Covers BOTH dispatch names 'computer_use' and 'computer'.
void set_computer_use_backend(RuntimeToolExecutor executor);
void clear_computer_use_backend();
[[nodiscard]] RuntimeToolExecutor& computer_use_backend();

/// c13b: structured config runtime tool backend (user-tier JSON settings).
void set_config_backend(RuntimeToolExecutor executor);
void clear_config_backend();
[[nodiscard]] RuntimeToolExecutor& config_backend();

// ── Agent tool factory ────────────────────────────────────────────────────
void set_agent_tool_factory(AgentToolFactory factory);
void clear_agent_tool_factory();
[[nodiscard]] AgentToolFactory& agent_tool_factory();

} // namespace loom::tools
