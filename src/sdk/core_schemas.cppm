/// @file core_schemas.cppm
/// @brief SDK Core Schemas - struct definitions for serializable SDK data types.
/// Migrated from src/entrypoints/sdk/coreSchemas.ts
///
/// These schemas are the single source of truth for SDK data types.
/// Provides struct definitions equivalent to the Zod schemas.
module;

#include <cstdint>

export module loom.sdk.core_schemas;

import std;

import loom.config.settings;       // SettingsScope (CONVERGE alias)
import loom.tools.agent_runtime;   // AgentDefinition (CONVERGE alias)

export namespace cc::sdk::core_schemas {

// ============================================================================
// Usage & Model Types
// ============================================================================

/// Model usage statistics
struct ModelUsage {
    int input_tokens = 0;
    int output_tokens = 0;
    int cache_read_input_tokens = 0;
    int cache_creation_input_tokens = 0;
    int web_search_requests = 0;
    double cost_usd = 0.0;
    int context_window = 0;
    int max_output_tokens = 0;
};

// ============================================================================
// Config Types
// ============================================================================

/// API key source
enum class ApiKeySource : std::uint8_t {
    User,
    Project,
    Org,
    Temporary,
    OAuth,
};

/// Config scope for settings — CONVERGED to cc::config::SettingsScope.
using ConfigScope = cc::config::SettingsScope;

/// SDK beta version
inline constexpr auto SDK_BETA = "context-1m-2025-08-07";

// ============================================================================
// MCP Server Status Types
// ============================================================================
struct McpToolAnnotation {
    std::optional<bool> read_only;
    std::optional<bool> destructive;
    std::optional<bool> open_world;
};

/// Tool provided by an MCP server
struct McpServerTool {
    std::string name;
    std::optional<std::string> description;
    std::optional<McpToolAnnotation> annotations;
};

/// Server info returned when connected
struct McpServerInfo {
    std::string name;
    std::string version;
};

/// Server capabilities
struct McpServerCapabilities {
    std::optional<std::unordered_map<std::string, std::string>> experimental;
};

/// Status of an MCP server connection
enum class McpConnectionStatus : std::uint8_t {
    Connected,
    Failed,
    NeedsAuth,
    Pending,
    Disabled,
};

/// Full MCP server status
struct McpServerStatus {
    std::string name;
    McpConnectionStatus status;
    std::optional<McpServerInfo> server_info;
    std::optional<std::string> error;
    std::optional<std::string> scope;
    std::optional<std::vector<McpServerTool>> tools;
    std::optional<McpServerCapabilities> capabilities;
};

// ============================================================================
// Hook Types
// ============================================================================

/// Hook event types
enum class HookEvent : std::uint8_t {
    PreToolUse,
    PostToolUse,
    PostToolUseFailure,
    Notification,
    UserPromptSubmit,
    SessionStart,
    SessionEnd,
    Stop,
    StopFailure,
    SubagentStart,
    SubagentStop,
    PreCompact,
    PostCompact,
    PermissionRequest,
    PermissionDenied,
    Setup,
    TeammateIdle,
    TaskCreated,
    TaskCompleted,
    Elicitation,
    ElicitationResult,
    ConfigChange,
    WorktreeCreate,
    WorktreeRemove,
    InstructionsLoaded,
    CwdChanged,
    FileChanged,
};

/// Exit reasons for session end
enum class ExitReason : std::uint8_t {
    Clear,
    Resume,
    Logout,
    PromptInputExit,
    Other,
    BypassPermissionsDisabled,
};

/// Base hook input fields (shared across all hook events)
struct BaseHookInput {
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode;
    std::optional<std::string> agent_id;
    std::optional<std::string> agent_type;
};

// ============================================================================
// Skill/Command Types
// ============================================================================

/// Slash command (skill) info
struct SlashCommand {
    std::string name;
    std::string description;
    std::string argument_hint;
};

/// Agent info
struct AgentInfo {
    std::string name;
    std::string description;
    std::optional<std::string> model;
};

/// Model info
struct ModelInfo {
    std::string value;
    std::string display_name;
    std::string description;
    std::optional<bool> supports_effort;
    std::optional<std::vector<std::string>> supported_effort_levels;
    std::optional<bool> supports_adaptive_thinking;
    std::optional<bool> supports_fast_mode;
    std::optional<bool> supports_auto_mode;
};

/// Account info
struct AccountInfo {
    std::optional<std::string> email;
    std::optional<std::string> organization;
    std::optional<std::string> subscription_type;
    std::optional<std::string> token_source;
    std::optional<std::string> api_key_source;
    std::optional<std::string> api_provider;  // "firstParty" | "bedrock" | "vertex" | "foundry"
};

/// Memory scope for agent definition
enum class AgentMemoryScope : std::uint8_t {
    User,
    Project,
    Local,
};

/// Agent definition for custom subagents — CONVERGED to
/// cc::tools::agent_runtime::AgentDefinition.
using AgentDefinition = cc::tools::agent_runtime::AgentDefinition;

/// Fast mode state
enum class FastModeState : std::uint8_t {
    Off,
    Cooldown,
    On,
};

// ============================================================================
// SDK Message Error Types
// ============================================================================

/// Assistant message error types
enum class SDKAssistantMessageError : std::uint8_t {
    AuthenticationFailed,
    BillingError,
    RateLimit,
    InvalidRequest,
    ServerError,
    Unknown,
    MaxOutputTokens,
};

// ============================================================================
// Settings Types
// ============================================================================

/// Setting source (file-based settings location) — CONVERGED to
/// cc::config::SettingsScope.
using SettingSource = cc::config::SettingsScope;

/// SDK Plugin configuration
struct SdkPluginConfig {
    static constexpr auto type = "local";
    std::string path;
};

/// Rewind files result
struct RewindFilesResult {
    bool can_rewind = false;
    std::optional<std::string> error;
    std::optional<std::vector<std::string>> files_changed;
    std::optional<int> insertions;
    std::optional<int> deletions;
};

} // namespace cc::sdk::core_schemas
