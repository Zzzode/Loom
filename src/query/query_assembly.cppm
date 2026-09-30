/// @file query_assembly.cppm
/// @brief Engine assembly recipe extracted from the server's
///        `detail::execute_native_query` (RFC 0001 cc-sdk phase 3, §2.1).
///
/// The 13-step in-process engine recipe (ConfigManager load -> config
/// resolution -> permission hook -> runtime backends -> ToolRegistry ->
/// missing-tool handler -> visible definitions -> dynamic tool providers ->
/// QueryEngine -> abort/permission callbacks -> seed -> run) lives here as an
/// assemble-only free function plus a shared settings->config resolver, so
/// both the server route (rank 13) and the SDK harness (rank 16) wrap the
/// SAME assembly step without an upward edge. This module is rank 10 (the
/// lowest rank that can see QueryEngine, register_runtime_tools,
/// install_runtime_backends, ToolPermissionHook, and session storage).
///
/// Assemble-only: `assemble(...)` builds the ToolPermissionHook, ToolRegistry
/// and QueryEngine, wires the abort/permission hooks, and seeds prior
/// messages — it does NOT run a turn. Callers run
/// `handle.engine().query(...)` / `stream_query(...)` themselves.
///
/// All non-trivial bodies live in the `query_assembly.cpp` implementation
/// unit (the same discipline as cc.query.query_engine): this interface keeps
/// only the configuration structs, the resolver/assemble declarations, and
/// the opaque AssemblyHandle PIMPL declaration.
module;

#include <cstdint>

export module cc.query.assembly;

import std;

import cc.types.types;       // cc::core::Result
import cc.config.config;     // cc::core::Settings
import cc.query.query_engine; // cc::core::QueryEngineConfig, QueryEngine
import cc.serdes.json;       // cc::utils::json::JsonVal (parse_session_message_value)
// ToolRegistry is needed for the register_extra_tools field type; the
// detector does not harvest the class name past cc.tools.tool's
// concept/requires blocks (same marker as runtime_message_delivery.cppm).
import cc.tools.tool;  // arch-check: keep-import
import cc.hooks.tool_permissions; // cc::hooks::AskUserResponseFn
import cc.tools.agent_types; // cc::tools::AgentLivePermissionCheckFn

export namespace cc::query {

/// Per-caller overrides layered on top of ConfigManager settings when
/// resolving the engine config. The server adapter fills these from the
/// DirectQueryRequest; the harness fills them from HarnessConfig. ONE
/// resolver (below) consumes them — the recipe's settings->config mapping
/// (server_routes.cppm:714-731) is not re-implemented per caller.
struct AssemblyOverrides {
    std::optional<std::string> requested_model;
    std::optional<std::string> api_key;  // bypasses settings.network.api_key
    std::optional<std::string> base_url;
    std::optional<std::string> wire_api; // "anthropic" | "openai"
    std::optional<std::string> cwd;
};

/// Shared settings -> QueryEngineConfig resolver (the recipe's :714-731
/// mapping). Does NOT hard-error on an empty api_key — the caller enforces
/// its own key policy: the server adapter requires a real key for the
/// direct-connect Anthropic path (server_routes.cppm:733-735); a harness
/// with a loopback/gateway base_url supplies a placeholder key and bypasses
/// the check. This is the single resolution path for both callers.
[[nodiscard]] cc::core::Result<cc::core::QueryEngineConfig> resolve_engine_config(
    const cc::core::Settings& settings, const AssemblyOverrides& overrides);

struct AssemblyConfig {
    cc::core::QueryEngineConfig engine;        // resolved via resolve_engine_config
    std::optional<std::filesystem::path> sessions_dir;     // enable session storage
    std::optional<std::filesystem::path> dump_prompts_dir;
    std::vector<std::string> prior_message_lines;          // resume seed (parsed at rank 10)
    std::shared_ptr<std::atomic_bool> cancel_flag;          // external abort
    /// Test seam: register extra tools (e.g. a mock permission-gated tool)
    /// into the registry before the config.tools snapshot is taken.
    std::function<void(cc::core::ToolRegistry&)> register_extra_tools;
};

struct AssemblyCallbacks {
    std::optional<cc::hooks::AskUserResponseFn> ask_user;  // permission bridge
    std::optional<cc::tools::AgentLivePermissionCheckFn> permission_check;
};

/// Reusable assembly result. Move-only PIMPL owning ToolPermissionHook +
/// ToolRegistry + QueryEngine (declaration/construction order: hook,
/// registry, engine — §2.3).
class AssemblyHandle {
public:
    AssemblyHandle(AssemblyHandle&&) noexcept;
    AssemblyHandle& operator=(AssemblyHandle&&) noexcept;
    ~AssemblyHandle();
    AssemblyHandle(const AssemblyHandle&) = delete;
    AssemblyHandle& operator=(const AssemblyHandle&) = delete;

    /// The assembled engine. Valid for the handle's lifetime. Callers run
    /// query()/stream_query()/abort()/restore_conversation() on it.
    [[nodiscard]] cc::core::QueryEngine& engine() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit AssemblyHandle(std::unique_ptr<Impl> impl) noexcept;

    friend cc::core::Result<AssemblyHandle> assemble(
        const AssemblyConfig& config, const AssemblyCallbacks& callbacks);
};

/// Assemble-only: take the resolved config -> build ToolPermissionHook ->
/// ToolRegistry (runtime tools + missing-tool backend) -> QueryEngine, wire
/// the abort/permission hooks, and seed prior messages. Does NOT run a turn.
/// (The caller loads ConfigManager and calls resolve_engine_config first —
/// the ConfigManager is a local, discarded after settings are read, not an
/// owned member.) assemble() also performs the recipe's step 8 internally:
/// it sets config.dynamic_tools_provider / config.mcp_input_schema_provider
/// to cc::tools::collect_mcp_tool_definitions / collect_mcp_input_schemas
/// (server_routes.cppm:804-810), so MCP tool discovery is preserved in the
/// re-expressed server route and not dropped by an implementer reading this
/// sketch literally.
[[nodiscard]] cc::core::Result<AssemblyHandle> assemble(
    const AssemblyConfig& config, const AssemblyCallbacks& callbacks);

/// Parse one messages.jsonl document into a cc::core::Message. The same
/// non-lossy reader assemble() uses for prior_message_lines, exported so
/// cc.sdk.harness::resume() can restore a prior session without
/// re-implementing the format (RFC 0001 cc-sdk phase 3, §2.3 resume path).
/// `message_from_json_value` returns cc::services::api::Message (a flat
/// struct), incompatible with restore_conversation's cc::core::Message
/// (5-member variant) with no existing converter — see the P3-assembly
/// deviation note. `index` seeds a fallback id when the document has none.
[[nodiscard]] std::optional<cc::core::Message> parse_session_message_value(
    cc::utils::json::JsonVal root, std::size_t index = 0);

} // namespace cc::query
