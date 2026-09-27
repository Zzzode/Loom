/// @file runtime_backends_port.cppm
/// @brief Unified runtime-backends seam — orchestration-owned executors.
///
/// Tool dispatch needs concrete backends that live above the tools layer
/// (lifted LSP/MCP/computer-use tools, the Agent tool, MCP connectivity), but
/// cc_tools cannot depend on cc_orchestration (rank 8 -> rank 9 would be an
/// upward edge). The concrete backends are installed once at process startup
/// via cc::orchestration::install_runtime_backends() and acquired through the
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
///   - the registry's missing-tool fallback (MCP per-server tools);
///   - the two MCP snapshot-derived providers (visible tool definitions and
///     their verbatim input schemas) consumed at request-build time;
///   - the Agent tool factory (5-arg make_agent_tool shape).
///
/// The MCP *snapshot sink* itself (set_mcp_snapshots_sink) deliberately does
/// NOT live here: its signature names cc::services::mcp::McpServerSnapshot,
/// and naming a services type from this rank-8 port would recreate a
/// tools->services area edge (the post-B15 target graph has zero). The sink
/// stays with the lifted cc.orchestration.tools.mcp module, where importing
/// cc.services.mcp.* is a rank-9 -> rank-7 downward edge. No raw pointer or
/// type-erased snapshot ever crosses this seam.
module;

export module cc.tools.runtime_backends.port;

import std;

import cc.types.types;
import cc.types.tool_types;
import cc.tools.tool;
import cc.tools.agent_types;

export namespace cc::tools {

/// Backs the runtime 'skill' tool's directory/plugin discovery. Receives the
/// raw ToolInput and returns the tool Result when it resolved a skill, or
/// std::nullopt to let the tools-side terminal manual SKILL.md walk run.
using SkillLoaderExecutor = std::function<
    std::optional<cc::core::Result<cc::core::ToolResult>>(
        const cc::core::ToolInput&)>;

/// Shape of every lifted runtime tool backend (the LSP/MCP/computer-use
/// sextet). Nullable: an empty std::function means "no deployment installed
/// one" and dispatch returns the terminal no-runtime-handler error.
using RuntimeToolExecutor =
    std::function<cc::core::Result<cc::core::ToolResult>(
        const cc::core::ToolInput&)>;

/// ToolRegistry missing-tool fallback. Installed by orchestration to route
/// unregistered tool names (per-server MCP tools) to connected servers.
using MissingToolBackend =
    std::function<cc::core::Result<cc::core::ToolResult>(
        std::string_view tool_name, const cc::core::ToolInput& input)>;

/// Snapshot-derived providers: the MCP tool definitions appended to
/// config.tools and their verbatim input schemas.
using McpToolDefinitionsProvider =
    std::function<std::vector<cc::core::ToolDefinition>()>;
using McpInputSchemasProvider =
    std::function<std::unordered_map<std::string, std::string>()>;

/// 5-arg Agent tool factory — the exact make_agent_tool shape owned by the
/// lifted cc.orchestration.agent module:
///   (AgentConfig, depth, ToolRegistry*, live permission checker,
///    background-hook-valid flag) -> owned ITool.
using AgentToolFactory = std::function<std::unique_ptr<cc::core::ITool>(
    AgentConfig,
    int depth,
    cc::core::ToolRegistry* registry,
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

// ── Missing-tool fallback ─────────────────────────────────────────────────
void set_missing_tool_backend(MissingToolBackend backend);
void clear_missing_tool_backend();
[[nodiscard]] MissingToolBackend& missing_tool_backend();

// ── MCP snapshot-derived providers ────────────────────────────────────────
void set_mcp_tool_definitions_provider(McpToolDefinitionsProvider provider);
void clear_mcp_tool_definitions_provider();
[[nodiscard]] McpToolDefinitionsProvider& mcp_tool_definitions_provider();

void set_mcp_input_schemas_provider(McpInputSchemasProvider provider);
void clear_mcp_input_schemas_provider();
[[nodiscard]] McpInputSchemasProvider& mcp_input_schemas_provider();

// ── Agent tool factory ────────────────────────────────────────────────────
void set_agent_tool_factory(AgentToolFactory factory);
void clear_agent_tool_factory();
[[nodiscard]] AgentToolFactory& agent_tool_factory();

} // namespace cc::tools
