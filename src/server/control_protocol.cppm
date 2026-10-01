/// @file control_protocol.cppm
/// @brief Canonical control-protocol wire DTOs with full JSON ser/de.
///
/// Owns the 21-subtype stdio/WS control protocol, the Stdin/Stdout envelopes,
/// the SDK stdout wire-message family, and the wire-closure types they
/// reference (SlashCommand, ModelInfo, AccountInfo, FastModeState, AgentInfo,
/// ModelUsage). Moved from the rank-16 loom.sdk island (RFC 0001 phases 2-3,
/// §1.2-1.3) so the live server/bridge (rank 13) can import the canonical
/// wire schema without an upward edge.
///
/// Ser/de follows the loom.server.types pattern: free X_to_json / X_from_json
/// plus ADL to_json / from_json overloads. The golden wire-compatibility gate
/// (tests/test_sdk_control_golden.cpp) freezes the exact bytes for every
/// spoken subtype.
module;

#include <cstdint>

export module loom.server.control_protocol;

import std;

import loom.serdes.json;
import loom.tools.agent_runtime;
import loom.model.effort;  // arch-check: keep-import (EffortLevel alias, line 39)

export namespace loom::server::control {

// ============================================================================
// Converged type aliases (canonical types — §1.2 move closure)
// ============================================================================

/// The engine's agent shape (loom.tools.agent_runtime). The SDK island twin is
/// converged to this alias so the moved DTOs reference the canonical type.
using AgentDefinition = loom::tools::agent_runtime::AgentDefinition;

/// Identical 4-value effort enum (loom.model.effort, in loom_utils).
using EffortLevel = loom::utils::EffortLevel;

// ============================================================================
// Wire-closure types (moved from loom.sdk.core_schemas — §1.2)
// ============================================================================

/// Slash command (skill) info. Carried by ControlInitializeResponse.commands.
struct SlashCommand {
    std::string name;
    std::string description;
    std::string argument_hint;
};

/// Agent info. Carried by ControlInitializeResponse.agents.
struct AgentInfo {
    std::string name;
    std::string description;
    std::optional<std::string> model;
};

/// Model info. Carried by ControlInitializeResponse.models.
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

/// Account info. Carried by ControlInitializeResponse.account.
struct AccountInfo {
    std::optional<std::string> email;
    std::optional<std::string> organization;
    std::optional<std::string> subscription_type;
    std::optional<std::string> token_source;
    std::optional<std::string> api_key_source;
    std::optional<std::string> api_provider;
};

/// Fast mode state. Carried by ControlInitializeResponse.fast_mode_state and
/// by SDKResultSuccess/SDKPostTurnSummaryMessage.
enum class FastModeState : std::uint8_t {
    Off,
    Cooldown,
    On,
};

/// Model usage statistics (whole-struct MOVE — all 8 fields retained, §1.2).
/// Carried by SDKResultSuccess/SDKResultError.modelUsage (camelCase on the
/// wire, matching the TS schema and the live emitters).
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
// Wire enums (no clean canonical twin — kept as wire types)
// ============================================================================

/// Wire permission mode (5 values; the canonical loom.tools.mode_validation
/// has 4 and different enumerator names — kept as a wire type).
enum class PermissionMode : std::uint8_t {
    Default,
    AcceptEdits,
    BypassPermissions,
    Plan,
    DontAsk,
};

/// Assistant message error types (7 wire values; loom::core::ErrorCode is
/// engine-internal with different numeric values — kept as a wire type).
enum class SDKAssistantMessageError : std::uint8_t {
    AuthenticationFailed,
    BillingError,
    RateLimit,
    InvalidRequest,
    ServerError,
    Unknown,
    MaxOutputTokens,
};

/// MCP connection status.
enum class McpConnectionStatus : std::uint8_t {
    Connected,
    Failed,
    NeedsAuth,
    Pending,
    Disabled,
};

/// Result error subtypes.
enum class ResultErrorSubtype : std::uint8_t {
    ErrorDuringExecution,
    ErrorMaxTurns,
    ErrorMaxBudgetUsd,
    ErrorMaxStructuredOutputRetries,
};

/// Elicitation action.
enum class ElicitationAction : std::uint8_t {
    Accept,
    Decline,
    Cancel,
};

/// Settings source name.
enum class SettingsSourceName : std::uint8_t {
    UserSettings,
    ProjectSettings,
    LocalSettings,
    FlagSettings,
    PolicySettings,
};

// ============================================================================
// Permission update family (wire shapes — no single canonical twin)
// ============================================================================

/// Destination for permission updates.
enum class PermissionUpdateDestination : std::uint8_t {
    UserSettings,
    ProjectSettings,
    LocalSettings,
    Session,
    CliArg,
};

/// Permission behavior.
enum class PermissionBehavior : std::uint8_t {
    Allow,
    Deny,
    Ask,
};

/// Permission rule value.
struct PermissionRuleValue {
    std::string tool_name;
    std::optional<std::string> rule_content;
};

/// Add rules permission update.
struct PermissionAddRules {
    std::vector<PermissionRuleValue> rules;
    PermissionBehavior behavior;
    PermissionUpdateDestination destination;
};

/// Replace rules permission update.
struct PermissionReplaceRules {
    std::vector<PermissionRuleValue> rules;
    PermissionBehavior behavior;
    PermissionUpdateDestination destination;
};

/// Remove rules permission update.
struct PermissionRemoveRules {
    std::vector<PermissionRuleValue> rules;
    PermissionBehavior behavior;
    PermissionUpdateDestination destination;
};

/// Set mode permission update.
struct PermissionSetMode {
    PermissionMode mode;
    PermissionUpdateDestination destination;
};

/// Add directories permission update.
struct PermissionAddDirectories {
    std::vector<std::string> directories;
    PermissionUpdateDestination destination;
};

/// Remove directories permission update.
struct PermissionRemoveDirectories {
    std::vector<std::string> directories;
    PermissionUpdateDestination destination;
};

/// Discriminated union of permission updates.
using PermissionUpdate = std::variant<
    PermissionAddRules,
    PermissionReplaceRules,
    PermissionRemoveRules,
    PermissionSetMode,
    PermissionAddDirectories,
    PermissionRemoveDirectories
>;

/// Classification of permission decision for telemetry.
enum class PermissionDecisionClassification : std::uint8_t {
    UserTemporary,
    UserPermanent,
    UserReject,
};

/// Permission allow result.
struct PermissionAllowResult {
    std::optional<std::unordered_map<std::string, std::string>> updated_input;
    std::optional<std::vector<PermissionUpdate>> updated_permissions;
    std::optional<std::string> tool_use_id;
    std::optional<PermissionDecisionClassification> decision_classification;
};

/// Permission deny result.
struct PermissionDenyResult {
    std::string message;
    std::optional<bool> interrupt;
    std::optional<std::string> tool_use_id;
    std::optional<PermissionDecisionClassification> decision_classification;
};

/// Permission result (allow or deny).
using PermissionResult = std::variant<PermissionAllowResult, PermissionDenyResult>;

// ============================================================================
// MCP config / status family (wire shapes)
// ============================================================================

/// Stdio transport MCP server config.
struct McpStdioServerConfig {
    std::string command;
    std::optional<std::vector<std::string>> args;
    std::optional<std::unordered_map<std::string, std::string>> env;
};

/// SSE transport MCP server config.
struct McpSSEServerConfig {
    std::string url;
    std::optional<std::unordered_map<std::string, std::string>> headers;
};

/// HTTP transport MCP server config.
struct McpHttpServerConfig {
    std::string url;
    std::optional<std::unordered_map<std::string, std::string>> headers;
};

/// SDK transport MCP server config.
struct McpSdkServerConfig {
    std::string name;
};

/// Union of process-transport MCP server configurations.
using McpServerConfig = std::variant<
    McpStdioServerConfig,
    McpSSEServerConfig,
    McpHttpServerConfig,
    McpSdkServerConfig
>;

/// Loom AI proxy server config (output-only).
struct McpLoomAIProxyServerConfig {
    std::string url;
    std::string id;
};

/// Tool annotation in MCP server status.
struct McpToolAnnotation {
    std::optional<bool> read_only;
    std::optional<bool> destructive;
    std::optional<bool> open_world;
};

/// Tool provided by an MCP server.
struct McpServerTool {
    std::string name;
    std::optional<std::string> description;
    std::optional<McpToolAnnotation> annotations;
};

/// Server info returned when connected.
struct McpServerInfo {
    std::string name;
    std::string version;
};

/// Server capabilities.
struct McpServerCapabilities {
    std::optional<std::unordered_map<std::string, std::string>> experimental;
};

/// Full MCP server status.
struct McpServerStatus {
    std::string name;
    McpConnectionStatus status;
    std::optional<McpServerInfo> server_info;
    std::optional<std::string> error;
    std::optional<McpServerConfig> config;
    std::optional<std::string> scope;
    std::optional<std::vector<McpServerTool>> tools;
    std::optional<McpServerCapabilities> capabilities;
};

// ============================================================================
// Control request subtypes (the 21-subtype protocol + raw passthrough)
// ============================================================================

/// Configuration for matching and routing hook callbacks.
struct HookCallbackMatcher {
    std::optional<std::string> matcher;
    std::vector<std::string> hook_callback_ids;
    std::optional<double> timeout;
};

/// Initializes the SDK session with hooks, MCP servers, and agent configuration.
struct ControlInitializeRequest {
    static constexpr auto subtype = "initialize";
    std::optional<std::unordered_map<std::string, std::vector<HookCallbackMatcher>>> hooks;
    std::optional<std::vector<std::string>> sdk_mcp_servers;
    std::optional<std::unordered_map<std::string, std::string>> json_schema;
    std::optional<std::string> system_prompt;
    std::optional<std::string> append_system_prompt;
    std::optional<std::unordered_map<std::string, AgentDefinition>> agents;
    std::optional<bool> prompt_suggestions;
    std::optional<bool> agent_progress_summaries;
};

/// Interrupts the currently running conversation turn.
struct ControlInterruptRequest {
    static constexpr auto subtype = "interrupt";
};

/// Requests permission to use a tool with the given input.
/// input_json is the canonical (parsed + re-serialized) JSON of the tool input.
struct ControlPermissionRequest {
    static constexpr auto subtype = "can_use_tool";
    std::string tool_name;
    std::string input_json;
    std::optional<std::vector<PermissionUpdate>> permission_suggestions;
    std::optional<std::string> blocked_path;
    std::optional<std::string> decision_reason;
    std::optional<std::string> title;
    std::optional<std::string> display_name;
    std::string tool_use_id;
    std::optional<std::string> agent_id;
    std::optional<std::string> description;
};

/// Sets the permission mode for tool execution handling.
struct ControlSetPermissionModeRequest {
    static constexpr auto subtype = "set_permission_mode";
    PermissionMode mode;
    std::optional<bool> ultraplan;
};

/// Sets the model to use for subsequent conversation turns.
struct ControlSetModelRequest {
    static constexpr auto subtype = "set_model";
    std::optional<std::string> model;
};

/// Sets the maximum number of thinking tokens for extended thinking.
struct ControlSetMaxThinkingTokensRequest {
    static constexpr auto subtype = "set_max_thinking_tokens";
    std::optional<int> max_thinking_tokens;
};

/// Requests the current status of all MCP server connections.
struct ControlMcpStatusRequest {
    static constexpr auto subtype = "mcp_status";
};

/// Requests a breakdown of current context window usage by category.
struct ControlGetContextUsageRequest {
    static constexpr auto subtype = "get_context_usage";
};

/// Rewinds file changes made since a specific user message.
struct ControlRewindFilesRequest {
    static constexpr auto subtype = "rewind_files";
    std::string user_message_id;
    std::optional<bool> dry_run;
};

/// Drops a pending async user message from the command queue.
struct ControlCancelAsyncMessageRequest {
    static constexpr auto subtype = "cancel_async_message";
    std::string message_uuid;
};

/// Seeds the readFileState cache with a path+mtime entry.
struct ControlSeedReadStateRequest {
    static constexpr auto subtype = "seed_read_state";
    std::string path;
    double mtime = 0;
};

/// Delivers a hook callback with its input data.
struct HookCallbackRequest {
    static constexpr auto subtype = "hook_callback";
    std::string callback_id;
    std::string tool_use_id;
    std::unordered_map<std::string, std::string> input;
};

/// Sends a JSON-RPC message to a specific MCP server.
struct ControlMcpMessageRequest {
    static constexpr auto subtype = "mcp_message";
    std::string server_name;
    std::string message;
};

/// Replaces the set of dynamically managed MCP servers.
struct ControlMcpSetServersRequest {
    static constexpr auto subtype = "mcp_set_servers";
    std::unordered_map<std::string, McpServerConfig> servers;
};

/// Reloads plugins from disk.
struct ControlReloadPluginsRequest {
    static constexpr auto subtype = "reload_plugins";
};

/// Reconnects a disconnected or failed MCP server.
struct ControlMcpReconnectRequest {
    static constexpr auto subtype = "mcp_reconnect";
    std::string server_name;
};

/// Enables or disables an MCP server.
struct ControlMcpToggleRequest {
    static constexpr auto subtype = "mcp_toggle";
    std::string server_name;
    bool enabled = false;
};

/// Stops a running task.
struct ControlStopTaskRequest {
    static constexpr auto subtype = "stop_task";
    std::string task_id;
};

/// Merges settings into the flag settings layer.
struct ControlApplyFlagSettingsRequest {
    static constexpr auto subtype = "apply_flag_settings";
    std::unordered_map<std::string, std::string> settings;
};

/// Returns the effective merged settings.
struct ControlGetSettingsRequest {
    static constexpr auto subtype = "get_settings";
};

/// Requests the SDK consumer to handle an MCP elicitation.
struct ControlElicitationRequest {
    static constexpr auto subtype = "elicitation";
    std::string mcp_server_name;
    std::string message;
    std::optional<std::string> mode;
    std::optional<std::string> url;
    std::optional<std::string> elicitation_id;
    std::optional<std::unordered_map<std::string, std::string>> requested_schema;
};

/// Raw passthrough for unrecognized subtypes (so the parser never fails on
/// a subtype the live binary doesn't model — e.g. "reload_config").
struct ControlRawRequest {
    std::string subtype;
    std::string raw_json;
};

// ============================================================================
// Control response subtypes
// ============================================================================

/// Response from session initialization.
/// Ser/de emission rules (frozen by the golden gate):
///   commands  — always emitted (even when empty)
///   agents    — emitted only when non-empty
///   output_style, available_output_styles, models, account — always emitted
///   pid       — emitted only when present
///   fast_mode_state — emitted only when present
struct ControlInitializeResponse {
    std::vector<SlashCommand> commands;
    std::vector<AgentInfo> agents;
    std::string output_style;
    std::vector<std::string> available_output_styles;
    std::vector<ModelInfo> models;
    AccountInfo account;
    std::optional<int> pid;
    std::optional<FastModeState> fast_mode_state;
};

/// Response containing MCP server connection status.
struct ControlMcpStatusResponse {
    std::vector<McpServerStatus> mcp_servers;
};

/// Result of a rewindFiles operation.
struct ControlRewindFilesResponse {
    bool can_rewind = false;
    std::optional<std::string> error;
    std::optional<std::vector<std::string>> files_changed;
    std::optional<int> insertions;
    std::optional<int> deletions;
};

/// Result of cancel_async_message operation.
struct ControlCancelAsyncMessageResponse {
    bool cancelled = false;
};

/// Result of replacing MCP servers.
struct ControlMcpSetServersResponse {
    std::vector<std::string> added;
    std::vector<std::string> removed;
    std::unordered_map<std::string, std::string> errors;
};

/// Plugin info.
struct PluginInfo {
    std::string name;
    std::string path;
    std::optional<std::string> source;
};

/// Response after plugin reload.
struct ControlReloadPluginsResponse {
    std::vector<SlashCommand> commands;
    std::vector<AgentInfo> agents;
    std::vector<PluginInfo> plugins;
    std::vector<McpServerStatus> mcp_servers;
    int error_count = 0;
};

/// A single source entry in settings.
struct SettingsSourceEntry {
    SettingsSourceName source;
    std::unordered_map<std::string, std::string> settings;
};

/// Applied runtime-resolved values.
struct AppliedSettings {
    std::string model;
    std::optional<EffortLevel> effort;
};

/// Response with effective merged settings.
struct ControlGetSettingsResponse {
    std::unordered_map<std::string, std::string> effective;
    std::vector<SettingsSourceEntry> sources;
    std::optional<AppliedSettings> applied;
};

/// Response for an elicitation request.
struct ControlElicitationResponse {
    ElicitationAction action;
    std::optional<std::unordered_map<std::string, std::string>> content;
};

// ============================================================================
// Context-usage sub-structs
// ============================================================================

struct ContextCategory {
    std::string name;
    int tokens = 0;
    std::string color;
    std::optional<bool> is_deferred;
};

struct ContextGridSquare {
    std::string color;
    bool is_filled = false;
    std::string category_name;
    int tokens = 0;
    double percentage = 0.0;
    double square_fullness = 0.0;
};

struct MemoryFileInfo {
    std::string path;
    std::string type;
    int tokens = 0;
};

struct McpToolInfo {
    std::string name;
    std::string server_name;
    int tokens = 0;
    std::optional<bool> is_loaded;
};

struct DeferredBuiltinToolInfo {
    std::string name;
    int tokens = 0;
    bool is_loaded = false;
};

struct SystemToolInfo {
    std::string name;
    int tokens = 0;
};

struct SystemPromptSection {
    std::string name;
    int tokens = 0;
};

struct AgentContextInfo {
    std::string agent_type;
    std::string source;
    int tokens = 0;
};

struct SlashCommandsContextInfo {
    int total_commands = 0;
    int included_commands = 0;
    int tokens = 0;
};

struct SkillFrontmatterInfo {
    std::string name;
    std::string source;
    int tokens = 0;
};

struct SkillsContextInfo {
    int total_skills = 0;
    int included_skills = 0;
    int tokens = 0;
    std::vector<SkillFrontmatterInfo> skill_frontmatter;
};

struct ToolCallsByTypeInfo {
    std::string name;
    int call_tokens = 0;
    int result_tokens = 0;
};

struct AttachmentsByTypeInfo {
    std::string name;
    int tokens = 0;
};

struct MessageBreakdown {
    int tool_call_tokens = 0;
    int tool_result_tokens = 0;
    int attachment_tokens = 0;
    int assistant_message_tokens = 0;
    int user_message_tokens = 0;
    std::vector<ToolCallsByTypeInfo> tool_calls_by_type;
    std::vector<AttachmentsByTypeInfo> attachments_by_type;
};

struct ApiUsage {
    int input_tokens = 0;
    int output_tokens = 0;
    int cache_creation_input_tokens = 0;
    int cache_read_input_tokens = 0;
};

/// Full context usage response.
struct ControlGetContextUsageResponse {
    std::vector<ContextCategory> categories;
    int total_tokens = 0;
    int max_tokens = 0;
    int raw_max_tokens = 0;
    double percentage = 0.0;
    std::vector<std::vector<ContextGridSquare>> grid_rows;
    std::string model;
    std::vector<MemoryFileInfo> memory_files;
    std::vector<McpToolInfo> mcp_tools;
    std::optional<std::vector<DeferredBuiltinToolInfo>> deferred_builtin_tools;
    std::optional<std::vector<SystemToolInfo>> system_tools;
    std::optional<std::vector<SystemPromptSection>> system_prompt_sections;
    std::vector<AgentContextInfo> agents;
    std::optional<SlashCommandsContextInfo> slash_commands;
    std::optional<SkillsContextInfo> skills;
    std::optional<int> auto_compact_threshold;
    bool is_auto_compact_enabled = false;
    std::optional<MessageBreakdown> message_breakdown;
    std::optional<ApiUsage> api_usage;
};

// ============================================================================
// Control wrappers
// ============================================================================

/// Union of all control request inner types.
using ControlRequestInner = std::variant<
    ControlInitializeRequest,
    ControlInterruptRequest,
    ControlPermissionRequest,
    ControlSetPermissionModeRequest,
    ControlSetModelRequest,
    ControlSetMaxThinkingTokensRequest,
    ControlMcpStatusRequest,
    ControlGetContextUsageRequest,
    ControlRewindFilesRequest,
    ControlCancelAsyncMessageRequest,
    ControlSeedReadStateRequest,
    HookCallbackRequest,
    ControlMcpMessageRequest,
    ControlMcpSetServersRequest,
    ControlReloadPluginsRequest,
    ControlMcpReconnectRequest,
    ControlMcpToggleRequest,
    ControlStopTaskRequest,
    ControlApplyFlagSettingsRequest,
    ControlGetSettingsRequest,
    ControlElicitationRequest,
    ControlRawRequest
>;

/// SDK Control Request wrapper.
struct ControlRequest {
    std::string request_id;
    ControlRequestInner request;
};

/// Successful control response.
/// response_json is the raw JSON of the inner "response" object (embedded
/// verbatim on the wire — it carries subtype-specific shapes like
/// {"interrupted":true} or {"behavior":"allow"}).
struct ControlSuccessResponse {
    std::string request_id;
    std::optional<std::string> response_json;
};

/// Error control response.
struct ControlErrorResponse {
    std::string request_id;
    std::string error;
    std::optional<std::vector<ControlRequest>> pending_permission_requests;
};

/// SDK Control Response wrapper.
/// session_id is present on bridge→server messages, absent on server→client.
struct ControlResponse {
    std::optional<std::string> session_id;
    std::variant<ControlSuccessResponse, ControlErrorResponse> response;
};

/// Cancels a currently open control request.
struct ControlCancelRequest {
    std::string request_id;
};

/// Keep-alive message to maintain WebSocket connection.
struct KeepAliveMessage {};

/// Updates environment variables at runtime.
struct UpdateEnvironmentVariablesMessage {
    std::unordered_map<std::string, std::string> variables;
};

// ============================================================================
// Runtime-only control request structs (from loom.sdk.control_types)
// ============================================================================

/// End session request.
struct SDKControlEndSessionRequest {
    static constexpr auto subtype = "end_session";
    std::optional<std::string> reason;
};

/// Channel enable request.
struct SDKControlChannelEnableRequest {
    static constexpr auto subtype = "channel_enable";
    std::string server_name;
};

/// MCP authenticate request.
struct SDKControlMcpAuthenticateRequest {
    static constexpr auto subtype = "mcp_authenticate";
    std::string server_name;
    std::string authorization_url;
    std::optional<std::unordered_map<std::string, std::string>> oauth;
};

/// MCP OAuth callback URL request.
struct SDKControlMcpOAuthCallbackUrlRequest {
    static constexpr auto subtype = "mcp_oauth_callback_url";
    std::string server_name;
    std::string callback_url;
};

// ============================================================================
// SDK stdout wire-message family (moved from loom.sdk.core_types)
// ============================================================================

/// SDK user message.
struct SDKUserMessage {
    std::unordered_map<std::string, std::string> message;
    std::optional<std::string> parent_tool_use_id;
    std::optional<bool> is_synthetic;
    std::optional<std::string> priority;
    std::optional<std::string> timestamp;
    std::optional<std::string> uuid;
    std::optional<std::string> session_id;
};

/// SDK user message replay.
struct SDKUserMessageReplay {
    std::unordered_map<std::string, std::string> message;
    std::optional<std::string> parent_tool_use_id;
    std::optional<bool> is_synthetic;
    std::string uuid;
    std::string session_id;
};

/// SDK assistant message.
struct SDKAssistantMessage {
    std::unordered_map<std::string, std::string> message;
    std::optional<std::string> parent_tool_use_id;
    std::optional<SDKAssistantMessageError> error;
    std::string uuid;
    std::string session_id;
};

/// SDK permission denial.
struct SDKPermissionDenial {
    std::string tool_name;
    std::string tool_use_id;
    std::unordered_map<std::string, std::string> tool_input;
};

/// SDK result success.
struct SDKResultSuccess {
    double duration_ms = 0;
    double duration_api_ms = 0;
    bool is_error = false;
    int num_turns = 0;
    std::string result;
    std::optional<std::string> stop_reason;
    double total_cost_usd = 0.0;
    // Opaque JSON (TS NonNullableUsagePlaceholder = z.unknown()): the live
    // wire nests objects (e.g. "server_tool_use":{"web_search_requests":0}),
    // so a typed map would silently drop them on parse.
    std::string usage_json = "{}";
    std::unordered_map<std::string, ModelUsage> model_usage;
    std::vector<SDKPermissionDenial> permission_denials;
    std::optional<std::string> structured_output;  // z.unknown() — raw JSON
    std::optional<FastModeState> fast_mode_state;
    std::string uuid;
    std::string session_id;
};

/// SDK result error.
struct SDKResultError {
    ResultErrorSubtype subtype;
    double duration_ms = 0;
    double duration_api_ms = 0;
    bool is_error = false;
    int num_turns = 0;
    std::optional<std::string> stop_reason;
    double total_cost_usd = 0.0;
    // Opaque JSON (TS NonNullableUsagePlaceholder = z.unknown()) — see
    // SDKResultSuccess::usage_json.
    std::string usage_json = "{}";
    std::unordered_map<std::string, ModelUsage> model_usage;
    std::vector<SDKPermissionDenial> permission_denials;
    std::vector<std::string> errors;
    std::optional<FastModeState> fast_mode_state;
    std::string uuid;
    std::string session_id;
};

/// SDK result message (success or error).
using SDKResultMessage = std::variant<SDKResultSuccess, SDKResultError>;

/// SDK system message (init and other subtypes).
struct SDKSystemMessage {
    std::string subtype;
    std::string uuid;
    std::string session_id;
    std::optional<std::string> model;
    std::optional<PermissionMode> permission_mode;
    std::optional<std::vector<std::string>> tools;
    std::optional<std::vector<std::string>> agents;
    std::optional<std::string> cwd;
    std::optional<std::string> output_style;
};

/// SDK partial assistant message (streaming).
struct SDKPartialAssistantMessage {
    std::string event_json;
    std::optional<std::string> parent_tool_use_id;
    std::string uuid;
    std::string session_id;
};

/// SDK compact boundary message.
struct SDKCompactBoundaryMessage {
    std::string trigger;
    int pre_tokens = 0;
    struct PreservedSegment {
        std::string head_uuid;
        std::string anchor_uuid;
        std::string tail_uuid;
    };
    std::optional<PreservedSegment> preserved_segment;
    std::string uuid;
    std::string session_id;
};

/// SDK status message.
struct SDKStatusMessage {
    std::optional<std::string> status;
    std::optional<PermissionMode> permission_mode;
    std::string uuid;
    std::string session_id;
};

/// SDK tool progress message.
struct SDKToolProgressMessage {
    std::string tool_use_id;
    std::string tool_name;
    std::optional<std::string> parent_tool_use_id;
    double elapsed_time_seconds = 0;
    std::optional<std::string> task_id;
    std::string uuid;
    std::string session_id;
};

/// SDK post-turn summary message.
struct SDKPostTurnSummaryMessage {
    std::string summarizes_uuid;
    std::string status_category;
    std::string status_detail;
    bool is_noteworthy = false;
    std::string title;
    std::string description;
    std::string recent_action;
    std::string needs_action;
    std::vector<std::string> artifact_urls;
    std::string uuid;
    std::string session_id;
};

/// SDK streamlined text message.
struct SDKStreamlinedTextMessage {
    std::string text;
    std::string session_id;
    std::string uuid;
};

/// SDK streamlined tool use summary message.
struct SDKStreamlinedToolUseSummaryMessage {
    std::string tool_summary;
    std::string session_id;
    std::string uuid;
};

/// Union of all SDK message types.
using SDKMessage = std::variant<
    SDKAssistantMessage,
    SDKUserMessage,
    SDKUserMessageReplay,
    SDKResultSuccess,
    SDKResultError,
    SDKSystemMessage,
    SDKPartialAssistantMessage,
    SDKCompactBoundaryMessage,
    SDKStatusMessage,
    SDKToolProgressMessage
>;

// ============================================================================
// Stdin/Stdout envelopes (moved from loom.sdk.control_types)
// ============================================================================

/// Messages written to stdout by the CLI.
using StdoutMessage = std::variant<
    SDKMessage,
    SDKStreamlinedTextMessage,
    SDKStreamlinedToolUseSummaryMessage,
    SDKPostTurnSummaryMessage,
    ControlResponse,
    ControlRequest,
    ControlCancelRequest,
    KeepAliveMessage
>;

/// Messages read from stdin by the CLI.
using StdinMessage = std::variant<
    SDKUserMessage,
    ControlRequest,
    ControlResponse,
    KeepAliveMessage,
    UpdateEnvironmentVariablesMessage
>;

// ============================================================================
// Ser/de — enum string helpers
// ============================================================================

namespace detail_serde {

[[nodiscard]] inline std::string_view fast_mode_state_to_str(FastModeState v) {
    switch (v) {
        case FastModeState::Off: return "off";
        case FastModeState::Cooldown: return "cooldown";
        case FastModeState::On: return "on";
    }
    return "off";
}
[[nodiscard]] inline std::optional<FastModeState> fast_mode_state_from_str(std::string_view s) {
    if (s == "off") return FastModeState::Off;
    if (s == "cooldown") return FastModeState::Cooldown;
    if (s == "on") return FastModeState::On;
    return std::nullopt;
}

[[nodiscard]] inline std::string_view permission_mode_to_str(PermissionMode v) {
    switch (v) {
        case PermissionMode::Default: return "default";
        case PermissionMode::AcceptEdits: return "acceptEdits";
        case PermissionMode::BypassPermissions: return "bypassPermissions";
        case PermissionMode::Plan: return "plan";
        case PermissionMode::DontAsk: return "dontAsk";
    }
    return "default";
}
[[nodiscard]] inline std::optional<PermissionMode> permission_mode_from_str(std::string_view s) {
    if (s == "default") return PermissionMode::Default;
    if (s == "acceptEdits") return PermissionMode::AcceptEdits;
    if (s == "bypassPermissions") return PermissionMode::BypassPermissions;
    if (s == "plan") return PermissionMode::Plan;
    if (s == "dontAsk") return PermissionMode::DontAsk;
    return std::nullopt;
}

[[nodiscard]] inline std::string_view mcp_connection_status_to_str(McpConnectionStatus v) {
    switch (v) {
        case McpConnectionStatus::Connected: return "connected";
        case McpConnectionStatus::Failed: return "failed";
        case McpConnectionStatus::NeedsAuth: return "needs_auth";
        case McpConnectionStatus::Pending: return "pending";
        case McpConnectionStatus::Disabled: return "disabled";
    }
    return "pending";
}
[[nodiscard]] inline std::optional<McpConnectionStatus> mcp_connection_status_from_str(std::string_view s) {
    if (s == "connected") return McpConnectionStatus::Connected;
    if (s == "failed") return McpConnectionStatus::Failed;
    if (s == "needs_auth") return McpConnectionStatus::NeedsAuth;
    if (s == "pending") return McpConnectionStatus::Pending;
    if (s == "disabled") return McpConnectionStatus::Disabled;
    return std::nullopt;
}

[[nodiscard]] inline std::string_view result_error_subtype_to_str(ResultErrorSubtype v) {
    switch (v) {
        case ResultErrorSubtype::ErrorDuringExecution: return "error_during_execution";
        case ResultErrorSubtype::ErrorMaxTurns: return "error_max_turns";
        case ResultErrorSubtype::ErrorMaxBudgetUsd: return "error_max_budget_usd";
        case ResultErrorSubtype::ErrorMaxStructuredOutputRetries: return "error_max_structured_output_retries";
    }
    return "error_during_execution";
}

[[nodiscard]] inline std::string_view elicitation_action_to_str(ElicitationAction v) {
    switch (v) {
        case ElicitationAction::Accept: return "accept";
        case ElicitationAction::Decline: return "decline";
        case ElicitationAction::Cancel: return "cancel";
    }
    return "accept";
}
[[nodiscard]] inline std::optional<ElicitationAction> elicitation_action_from_str(std::string_view s) {
    if (s == "accept") return ElicitationAction::Accept;
    if (s == "decline") return ElicitationAction::Decline;
    if (s == "cancel") return ElicitationAction::Cancel;
    return std::nullopt;
}

[[nodiscard]] inline std::string_view settings_source_name_to_str(SettingsSourceName v) {
    switch (v) {
        case SettingsSourceName::UserSettings: return "userSettings";
        case SettingsSourceName::ProjectSettings: return "projectSettings";
        case SettingsSourceName::LocalSettings: return "localSettings";
        case SettingsSourceName::FlagSettings: return "flagSettings";
        case SettingsSourceName::PolicySettings: return "policySettings";
    }
    return "userSettings";
}

[[nodiscard]] inline std::string_view permission_update_destination_to_str(PermissionUpdateDestination v) {
    switch (v) {
        case PermissionUpdateDestination::UserSettings: return "userSettings";
        case PermissionUpdateDestination::ProjectSettings: return "projectSettings";
        case PermissionUpdateDestination::LocalSettings: return "localSettings";
        case PermissionUpdateDestination::Session: return "session";
        case PermissionUpdateDestination::CliArg: return "cliArg";
    }
    return "userSettings";
}

[[nodiscard]] inline std::string_view permission_behavior_to_str(PermissionBehavior v) {
    switch (v) {
        case PermissionBehavior::Allow: return "allow";
        case PermissionBehavior::Deny: return "deny";
        case PermissionBehavior::Ask: return "ask";
    }
    return "allow";
}

[[nodiscard]] inline std::string_view sdk_assistant_message_error_to_str(SDKAssistantMessageError v) {
    switch (v) {
        case SDKAssistantMessageError::AuthenticationFailed: return "authentication_failed";
        case SDKAssistantMessageError::BillingError: return "billing_error";
        case SDKAssistantMessageError::RateLimit: return "rate_limit";
        case SDKAssistantMessageError::InvalidRequest: return "invalid_request";
        case SDKAssistantMessageError::ServerError: return "server_error";
        case SDKAssistantMessageError::Unknown: return "unknown";
        case SDKAssistantMessageError::MaxOutputTokens: return "max_output_tokens";
    }
    return "unknown";
}

/// Read a string field; returns empty string when missing or wrong type.
[[nodiscard]] inline std::string read_string(loom::utils::json::JsonVal obj, std::string_view key) {
    auto v = obj.get(key);
    return v.is_str() ? std::string(v.as_str()) : std::string{};
}

/// Read an optional string field.
[[nodiscard]] inline std::optional<std::string> read_optional_string(
    loom::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_str()) return std::string(v.as_str());
    return std::nullopt;
}

/// Read an optional int field.
[[nodiscard]] inline std::optional<int64_t> read_optional_int(
    loom::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_num()) return v.as_int();
    return std::nullopt;
}

/// Read an optional double field (yyjson stores 30.0 as a real, not an int;
/// read_optional_int would return 0 for it).
[[nodiscard]] inline std::optional<double> read_optional_double(
    loom::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_num()) return v.as_double();
    return std::nullopt;
}

/// Read an optional bool field.
[[nodiscard]] inline std::optional<bool> read_optional_bool(
    loom::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_bool()) return v.as_bool();
    return std::nullopt;
}

/// Read a string vector field.
[[nodiscard]] inline std::vector<std::string> read_string_vec(
    loom::utils::json::JsonVal obj, std::string_view key
) {
    std::vector<std::string> out;
    auto arr = obj.get(key);
    if (arr.is_arr()) {
        arr.iter([&](loom::utils::json::JsonVal el) {
            if (el.is_str()) out.emplace_back(el.as_str());
        });
    }
    return out;
}

} // namespace detail_serde

/// Public string conversion for PermissionMode (the detail_serde
/// version is module-internal; bridge_messaging needs the wire string).
[[nodiscard]] inline std::string_view permission_mode_to_str(PermissionMode v) {
    return detail_serde::permission_mode_to_str(v);
}

// ============================================================================
// Ser/de — wire-closure types
// ============================================================================

[[nodiscard]] std::string SlashCommand_to_json(const SlashCommand& v);
[[nodiscard]] std::expected<SlashCommand, std::string> SlashCommand_from_json(std::string_view raw);
inline std::string to_json(const SlashCommand& v) { return SlashCommand_to_json(v); }
inline std::expected<SlashCommand, std::string> from_json(std::string_view raw, SlashCommand*) {
    return SlashCommand_from_json(raw);
}

[[nodiscard]] std::string AgentInfo_to_json(const AgentInfo& v);
[[nodiscard]] std::expected<AgentInfo, std::string> AgentInfo_from_json(std::string_view raw);
inline std::string to_json(const AgentInfo& v) { return AgentInfo_to_json(v); }
inline std::expected<AgentInfo, std::string> from_json(std::string_view raw, AgentInfo*) {
    return AgentInfo_from_json(raw);
}

[[nodiscard]] std::string ModelInfo_to_json(const ModelInfo& v);
[[nodiscard]] std::expected<ModelInfo, std::string> ModelInfo_from_json(std::string_view raw);
inline std::string to_json(const ModelInfo& v) { return ModelInfo_to_json(v); }
inline std::expected<ModelInfo, std::string> from_json(std::string_view raw, ModelInfo*) {
    return ModelInfo_from_json(raw);
}

[[nodiscard]] std::string AccountInfo_to_json(const AccountInfo& v);
[[nodiscard]] std::expected<AccountInfo, std::string> AccountInfo_from_json(std::string_view raw);
inline std::string to_json(const AccountInfo& v) { return AccountInfo_to_json(v); }
inline std::expected<AccountInfo, std::string> from_json(std::string_view raw, AccountInfo*) {
    return AccountInfo_from_json(raw);
}

// Wire spellings are camelCase, matching the TS ModelUsageSchema
// (coreSchemas.ts at b69b59b^) and the live emitters — the C++ field names
// stay snake_case; only the JSON keys differ.
[[nodiscard]] std::string ModelUsage_to_json(const ModelUsage& v);
[[nodiscard]] std::expected<ModelUsage, std::string> ModelUsage_from_json(std::string_view raw);
inline std::string to_json(const ModelUsage& v) { return ModelUsage_to_json(v); }
inline std::expected<ModelUsage, std::string> from_json(std::string_view raw, ModelUsage*) {
    return ModelUsage_from_json(raw);
}

// ============================================================================
// Ser/de — control request subtypes
// ============================================================================

// ── ControlInitializeRequest ────────────────────────────────────────────────

[[nodiscard]] std::string ControlInitializeRequest_to_json(const ControlInitializeRequest& v);

// ── ControlInterruptRequest ─────────────────────────────────────────────────

[[nodiscard]] std::string ControlInterruptRequest_to_json(const ControlInterruptRequest&);

// ── ControlPermissionRequest (golden-gate) ──────────────────────────────────

[[nodiscard]] std::string ControlPermissionRequest_to_json(const ControlPermissionRequest& v);
[[nodiscard]] std::expected<ControlPermissionRequest, std::string> ControlPermissionRequest_from_json(loom::utils::json::JsonVal inner);

// ── ControlSetPermissionModeRequest (golden-gate) ───────────────────────────

[[nodiscard]] std::string ControlSetPermissionModeRequest_to_json(     const ControlSetPermissionModeRequest& v );
[[nodiscard]] std::expected<ControlSetPermissionModeRequest, std::string> ControlSetPermissionModeRequest_from_json(loom::utils::json::JsonVal inner);

// ── ControlSetModelRequest (golden-gate) ────────────────────────────────────

[[nodiscard]] std::string ControlSetModelRequest_to_json(const ControlSetModelRequest& v);
[[nodiscard]] std::expected<ControlSetModelRequest, std::string> ControlSetModelRequest_from_json(loom::utils::json::JsonVal inner);

// ── ControlSetMaxThinkingTokensRequest (golden-gate) ────────────────────────

[[nodiscard]] std::string ControlSetMaxThinkingTokensRequest_to_json(     const ControlSetMaxThinkingTokensRequest& v );
[[nodiscard]] std::expected<ControlSetMaxThinkingTokensRequest, std::string> ControlSetMaxThinkingTokensRequest_from_json(loom::utils::json::JsonVal inner);

// ── ControlRawRequest (unknown subtypes) ────────────────────────────────────

[[nodiscard]] std::string ControlRawRequest_to_json(const ControlRawRequest& v);

// ── McpServerConfig variant ─────────────────────────────────────────────────

[[nodiscard]] std::string McpStdioServerConfig_to_json(const McpStdioServerConfig& v);
[[nodiscard]] std::expected<McpStdioServerConfig, std::string> McpStdioServerConfig_from_json(loom::utils::json::JsonVal o);

[[nodiscard]] std::string McpSSEServerConfig_to_json(const McpSSEServerConfig& v);
[[nodiscard]] std::expected<McpSSEServerConfig, std::string> McpSSEServerConfig_from_json(loom::utils::json::JsonVal o);

[[nodiscard]] std::string McpHttpServerConfig_to_json(const McpHttpServerConfig& v);
[[nodiscard]] std::expected<McpHttpServerConfig, std::string> McpHttpServerConfig_from_json(loom::utils::json::JsonVal o);

[[nodiscard]] std::string McpSdkServerConfig_to_json(const McpSdkServerConfig& v);
[[nodiscard]] std::expected<McpSdkServerConfig, std::string> McpSdkServerConfig_from_json(loom::utils::json::JsonVal o);

[[nodiscard]] std::string McpServerConfig_to_json(const McpServerConfig& v);
[[nodiscard]] std::expected<McpServerConfig, std::string> McpServerConfig_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<McpServerConfig, std::string> McpServerConfig_from_json(std::string_view raw);

// ── ControlInitializeRequest (full parse) ───────────────────────────────────

[[nodiscard]] std::expected<ControlInitializeRequest, std::string> ControlInitializeRequest_from_json(loom::utils::json::JsonVal inner);

// ── Remaining control request subtypes ──────────────────────────────────────

[[nodiscard]] std::string ControlMcpStatusRequest_to_json(     const ControlMcpStatusRequest& );

[[nodiscard]] std::string ControlGetContextUsageRequest_to_json(     const ControlGetContextUsageRequest& );

[[nodiscard]] std::string ControlRewindFilesRequest_to_json(     const ControlRewindFilesRequest& v );
[[nodiscard]] std::expected<ControlRewindFilesRequest, std::string> ControlRewindFilesRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlCancelAsyncMessageRequest_to_json(     const ControlCancelAsyncMessageRequest& v );
[[nodiscard]] std::expected<ControlCancelAsyncMessageRequest, std::string> ControlCancelAsyncMessageRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlSeedReadStateRequest_to_json(     const ControlSeedReadStateRequest& v );
[[nodiscard]] std::expected<ControlSeedReadStateRequest, std::string> ControlSeedReadStateRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string HookCallbackRequest_to_json(const HookCallbackRequest& v);
[[nodiscard]] std::expected<HookCallbackRequest, std::string> HookCallbackRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlMcpMessageRequest_to_json(     const ControlMcpMessageRequest& v );
[[nodiscard]] std::expected<ControlMcpMessageRequest, std::string> ControlMcpMessageRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlMcpSetServersRequest_to_json(     const ControlMcpSetServersRequest& v );
[[nodiscard]] std::expected<ControlMcpSetServersRequest, std::string> ControlMcpSetServersRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlReloadPluginsRequest_to_json(     const ControlReloadPluginsRequest& );

[[nodiscard]] std::string ControlMcpReconnectRequest_to_json(     const ControlMcpReconnectRequest& v );
[[nodiscard]] std::expected<ControlMcpReconnectRequest, std::string> ControlMcpReconnectRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlMcpToggleRequest_to_json(     const ControlMcpToggleRequest& v );
[[nodiscard]] std::expected<ControlMcpToggleRequest, std::string> ControlMcpToggleRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlStopTaskRequest_to_json(     const ControlStopTaskRequest& v );
[[nodiscard]] std::expected<ControlStopTaskRequest, std::string> ControlStopTaskRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlApplyFlagSettingsRequest_to_json(     const ControlApplyFlagSettingsRequest& v );
[[nodiscard]] std::expected<ControlApplyFlagSettingsRequest, std::string> ControlApplyFlagSettingsRequest_from_json(loom::utils::json::JsonVal inner);

[[nodiscard]] std::string ControlGetSettingsRequest_to_json(     const ControlGetSettingsRequest& );

[[nodiscard]] std::string ControlElicitationRequest_to_json(     const ControlElicitationRequest& v );
[[nodiscard]] std::expected<ControlElicitationRequest, std::string> ControlElicitationRequest_from_json(loom::utils::json::JsonVal inner);

// ── Variant dispatch: ControlRequestInner → JSON ───────────────────────────

[[nodiscard]] std::string control_request_inner_to_json(const ControlRequestInner& inner);

/// Extract the subtype string from any ControlRequestInner alternative.
[[nodiscard]] std::string control_request_subtype_str(const ControlRequestInner& inner);

/// Parse a ControlRequestInner from a subtype string + the inner JSON object.
/// Unknown subtypes produce a ControlRawRequest (never a parse failure).
[[nodiscard]] ControlRequestInner parse_control_request_inner(     std::string_view subtype,     loom::utils::json::JsonVal inner );

// ============================================================================
// Ser/de — control response subtypes
// ============================================================================

// ── ControlInitializeResponse (golden-gate) ─────────────────────────────────

[[nodiscard]] std::string ControlInitializeResponse_to_json(     const ControlInitializeResponse& v );
[[nodiscard]] std::expected<ControlInitializeResponse, std::string> ControlInitializeResponse_from_json(loom::utils::json::JsonVal inner);
[[nodiscard]] std::expected<ControlInitializeResponse, std::string> ControlInitializeResponse_from_json(std::string_view raw);

// ============================================================================
// Ser/de — control wrappers (golden-gate)
// ============================================================================

// ── ControlSuccessResponse ──────────────────────────────────────────────────

[[nodiscard]] std::string ControlSuccessResponse_to_json(const ControlSuccessResponse& v);
[[nodiscard]] std::expected<ControlSuccessResponse, std::string> ControlSuccessResponse_from_json(loom::utils::json::JsonVal inner);
[[nodiscard]] std::expected<ControlSuccessResponse, std::string> ControlSuccessResponse_from_json(std::string_view raw);

// ── ControlErrorResponse ────────────────────────────────────────────────────

[[nodiscard]] std::string ControlErrorResponse_to_json(const ControlErrorResponse& v);
[[nodiscard]] std::expected<ControlErrorResponse, std::string> ControlErrorResponse_from_json(loom::utils::json::JsonVal inner);
[[nodiscard]] std::expected<ControlErrorResponse, std::string> ControlErrorResponse_from_json(std::string_view raw);

// ── ControlRequest envelope ─────────────────────────────────────────────────

[[nodiscard]] std::string ControlRequest_to_json(const ControlRequest& v);
[[nodiscard]] std::expected<ControlRequest, std::string> ControlRequest_from_json(     std::string_view raw );
inline std::string to_json(const ControlRequest& v) { return ControlRequest_to_json(v); }
inline std::expected<ControlRequest, std::string> from_json(std::string_view raw, ControlRequest*) {
    return ControlRequest_from_json(raw);
}

// ── ControlResponse envelope ────────────────────────────────────────────────

namespace detail_serde {

[[nodiscard]] std::string control_response_inner_to_json(     const std::variant<ControlSuccessResponse, ControlErrorResponse>& inner );

} // namespace detail_serde

[[nodiscard]] std::string ControlResponse_to_json(const ControlResponse& v);
[[nodiscard]] std::expected<ControlResponse, std::string> ControlResponse_from_json(     std::string_view raw );
inline std::string to_json(const ControlResponse& v) { return ControlResponse_to_json(v); }
inline std::expected<ControlResponse, std::string> from_json(std::string_view raw, ControlResponse*) {
    return ControlResponse_from_json(raw);
}

// ============================================================================
// Ser/de — SDK stdout wire-message family
// ============================================================================

namespace detail_serde {

[[nodiscard]] std::string string_map_to_json(     const std::unordered_map<std::string, std::string>& m );

[[nodiscard]] std::unordered_map<std::string, std::string> string_map_from_json(     loom::utils::json::JsonVal o );

[[nodiscard]] std::optional<SDKAssistantMessageError> sdk_assistant_message_error_from_str(std::string_view s);

[[nodiscard]] std::optional<ResultErrorSubtype> result_error_subtype_from_str(std::string_view s);

} // namespace detail_serde

// ── SDKPermissionDenial ─────────────────────────────────────────────────────

[[nodiscard]] std::string SDKPermissionDenial_to_json(const SDKPermissionDenial& v);
[[nodiscard]] std::expected<SDKPermissionDenial, std::string> SDKPermissionDenial_from_json(loom::utils::json::JsonVal o);

// ── SDKUserMessage ──────────────────────────────────────────────────────────

[[nodiscard]] std::string SDKUserMessage_to_json(const SDKUserMessage& v);
[[nodiscard]] std::expected<SDKUserMessage, std::string> SDKUserMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKUserMessage, std::string> SDKUserMessage_from_json(std::string_view raw);

// ── SDKUserMessageReplay ────────────────────────────────────────────────────

[[nodiscard]] std::string SDKUserMessageReplay_to_json(const SDKUserMessageReplay& v);
[[nodiscard]] std::expected<SDKUserMessageReplay, std::string> SDKUserMessageReplay_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKUserMessageReplay, std::string> SDKUserMessageReplay_from_json(std::string_view raw);

// ── SDKAssistantMessage ─────────────────────────────────────────────────────

[[nodiscard]] std::string SDKAssistantMessage_to_json(const SDKAssistantMessage& v);
[[nodiscard]] std::expected<SDKAssistantMessage, std::string> SDKAssistantMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKAssistantMessage, std::string> SDKAssistantMessage_from_json(std::string_view raw);

// ── SDKResultSuccess ────────────────────────────────────────────────────────

[[nodiscard]] std::string SDKResultSuccess_to_json(const SDKResultSuccess& v);
[[nodiscard]] std::expected<SDKResultSuccess, std::string> SDKResultSuccess_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKResultSuccess, std::string> SDKResultSuccess_from_json(std::string_view raw);

// ── SDKResultError ──────────────────────────────────────────────────────────

[[nodiscard]] std::string SDKResultError_to_json(const SDKResultError& v);
[[nodiscard]] std::expected<SDKResultError, std::string> SDKResultError_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKResultError, std::string> SDKResultError_from_json(std::string_view raw);

// ── SDKSystemMessage ────────────────────────────────────────────────────────

[[nodiscard]] std::string SDKSystemMessage_to_json(const SDKSystemMessage& v);
[[nodiscard]] std::expected<SDKSystemMessage, std::string> SDKSystemMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKSystemMessage, std::string> SDKSystemMessage_from_json(std::string_view raw);

// ── SDKPartialAssistantMessage ──────────────────────────────────────────────

[[nodiscard]] std::string SDKPartialAssistantMessage_to_json(     const SDKPartialAssistantMessage& v );
[[nodiscard]] std::expected<SDKPartialAssistantMessage, std::string> SDKPartialAssistantMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKPartialAssistantMessage, std::string> SDKPartialAssistantMessage_from_json(std::string_view raw);

// ── SDKCompactBoundaryMessage ───────────────────────────────────────────────

[[nodiscard]] std::string SDKCompactBoundaryMessage_to_json(     const SDKCompactBoundaryMessage& v );
[[nodiscard]] std::expected<SDKCompactBoundaryMessage, std::string> SDKCompactBoundaryMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKCompactBoundaryMessage, std::string> SDKCompactBoundaryMessage_from_json(std::string_view raw);

// ── SDKStatusMessage ────────────────────────────────────────────────────────

[[nodiscard]] std::string SDKStatusMessage_to_json(const SDKStatusMessage& v);
[[nodiscard]] std::expected<SDKStatusMessage, std::string> SDKStatusMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKStatusMessage, std::string> SDKStatusMessage_from_json(std::string_view raw);

// ── SDKToolProgressMessage ──────────────────────────────────────────────────

[[nodiscard]] std::string SDKToolProgressMessage_to_json(     const SDKToolProgressMessage& v );
[[nodiscard]] std::expected<SDKToolProgressMessage, std::string> SDKToolProgressMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKToolProgressMessage, std::string> SDKToolProgressMessage_from_json(std::string_view raw);

// ── SDKPostTurnSummaryMessage ───────────────────────────────────────────────

[[nodiscard]] std::string SDKPostTurnSummaryMessage_to_json(     const SDKPostTurnSummaryMessage& v );
[[nodiscard]] std::expected<SDKPostTurnSummaryMessage, std::string> SDKPostTurnSummaryMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKPostTurnSummaryMessage, std::string> SDKPostTurnSummaryMessage_from_json(std::string_view raw);

// ── SDKStreamlinedTextMessage ───────────────────────────────────────────────

[[nodiscard]] std::string SDKStreamlinedTextMessage_to_json(     const SDKStreamlinedTextMessage& v );
[[nodiscard]] std::expected<SDKStreamlinedTextMessage, std::string> SDKStreamlinedTextMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKStreamlinedTextMessage, std::string> SDKStreamlinedTextMessage_from_json(std::string_view raw);

// ── SDKStreamlinedToolUseSummaryMessage ─────────────────────────────────────

[[nodiscard]] std::string SDKStreamlinedToolUseSummaryMessage_to_json(     const SDKStreamlinedToolUseSummaryMessage& v );
[[nodiscard]] std::expected<SDKStreamlinedToolUseSummaryMessage, std::string> SDKStreamlinedToolUseSummaryMessage_from_json(loom::utils::json::JsonVal o);
[[nodiscard]] std::expected<SDKStreamlinedToolUseSummaryMessage, std::string> SDKStreamlinedToolUseSummaryMessage_from_json(std::string_view raw);

// ── SDKMessage variant dispatch ─────────────────────────────────────────────

[[nodiscard]] std::string SDKMessage_to_json(const SDKMessage& v);

[[nodiscard]] std::expected<SDKMessage, std::string> SDKMessage_from_json(std::string_view raw);

// ============================================================================
// Ser/de — envelopes and misc
// ============================================================================

// ── ControlCancelRequest ────────────────────────────────────────────────────

[[nodiscard]] std::string ControlCancelRequest_to_json(const ControlCancelRequest& v);
[[nodiscard]] std::expected<ControlCancelRequest, std::string> ControlCancelRequest_from_json(std::string_view raw);
inline std::string to_json(const ControlCancelRequest& v) {
    return ControlCancelRequest_to_json(v);
}
inline std::expected<ControlCancelRequest, std::string> from_json(
    std::string_view raw, ControlCancelRequest*
) {
    return ControlCancelRequest_from_json(raw);
}

// ── KeepAliveMessage ────────────────────────────────────────────────────────

[[nodiscard]] std::string KeepAliveMessage_to_json(const KeepAliveMessage&);
[[nodiscard]] std::expected<KeepAliveMessage, std::string> KeepAliveMessage_from_json(std::string_view raw);
inline std::string to_json(const KeepAliveMessage& v) {
    return KeepAliveMessage_to_json(v);
}
inline std::expected<KeepAliveMessage, std::string> from_json(
    std::string_view raw, KeepAliveMessage*
) {
    return KeepAliveMessage_from_json(raw);
}

// ── UpdateEnvironmentVariablesMessage ───────────────────────────────────────

[[nodiscard]] std::string UpdateEnvironmentVariablesMessage_to_json(     const UpdateEnvironmentVariablesMessage& v );
[[nodiscard]] std::expected<UpdateEnvironmentVariablesMessage, std::string> UpdateEnvironmentVariablesMessage_from_json(std::string_view raw);
inline std::string to_json(const UpdateEnvironmentVariablesMessage& v) {
    return UpdateEnvironmentVariablesMessage_to_json(v);
}
inline std::expected<UpdateEnvironmentVariablesMessage, std::string> from_json(
    std::string_view raw, UpdateEnvironmentVariablesMessage*
) {
    return UpdateEnvironmentVariablesMessage_from_json(raw);
}

// ── StdoutMessage envelope ──────────────────────────────────────────────────

[[nodiscard]] std::string StdoutMessage_to_json(const StdoutMessage& v);

[[nodiscard]] std::expected<StdoutMessage, std::string> StdoutMessage_from_json(std::string_view raw);

// ── StdinMessage envelope ───────────────────────────────────────────────────

[[nodiscard]] std::string StdinMessage_to_json(const StdinMessage& v);

[[nodiscard]] std::expected<StdinMessage, std::string> StdinMessage_from_json(std::string_view raw);

} // namespace loom::server::control
