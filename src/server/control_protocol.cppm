/// @file control_protocol.cppm
/// @brief Canonical control-protocol wire DTOs with full JSON ser/de.
///
/// Owns the 21-subtype stdio/WS control protocol, the Stdin/Stdout envelopes,
/// the SDK stdout wire-message family, and the wire-closure types they
/// reference (SlashCommand, ModelInfo, AccountInfo, FastModeState, AgentInfo,
/// ModelUsage). Moved from the rank-16 cc.sdk island (RFC 0001 phases 2-3,
/// §1.2-1.3) so the live server/bridge (rank 13) can import the canonical
/// wire schema without an upward edge.
///
/// Ser/de follows the cc.server.types pattern: free X_to_json / X_from_json
/// plus ADL to_json / from_json overloads. The golden wire-compatibility gate
/// (tests/test_sdk_control_golden.cpp) freezes the exact bytes for every
/// spoken subtype.
module;

#include <cstdint>

export module cc.server.control_protocol;

import std;

import cc.serdes.json;
import cc.tools.agent_runtime;
import cc.model.effort;  // arch-check: keep-import (EffortLevel alias, line 39)

export namespace cc::server::control {

// ============================================================================
// Converged type aliases (canonical types — §1.2 move closure)
// ============================================================================

/// The engine's agent shape (cc.tools.agent_runtime). The SDK island twin is
/// converged to this alias so the moved DTOs reference the canonical type.
using AgentDefinition = cc::tools::agent_runtime::AgentDefinition;

/// Identical 4-value effort enum (cc.model.effort, in cc_utils).
using EffortLevel = cc::utils::EffortLevel;

// ============================================================================
// Wire-closure types (moved from cc.sdk.core_schemas — §1.2)
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
/// Carried by SDKResultSuccess/SDKResultError.model_usage.
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

/// Wire permission mode (5 values; the canonical cc.tools.mode_validation
/// has 4 and different enumerator names — kept as a wire type).
enum class PermissionMode : std::uint8_t {
    Default,
    AcceptEdits,
    BypassPermissions,
    Plan,
    DontAsk,
};

/// Assistant message error types (7 wire values; cc::core::ErrorCode is
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
// Runtime-only control request structs (from cc.sdk.control_types)
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
// SDK stdout wire-message family (moved from cc.sdk.core_types)
// ============================================================================

/// Non-nullable usage (all fields guaranteed present on the wire).
/// This is a wire shape — the field set differs from cc::core::TokenUsage
/// (cache_creation_input_tokens vs cache_creation_tokens, plus server_tool_use).
using NonNullableUsage = std::unordered_map<std::string, int>;

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
    NonNullableUsage usage;
    std::unordered_map<std::string, ModelUsage> model_usage;
    std::vector<SDKPermissionDenial> permission_denials;
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
    NonNullableUsage usage;
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
// Stdin/Stdout envelopes (moved from cc.sdk.control_types)
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
[[nodiscard]] inline std::string read_string(cc::utils::json::JsonVal obj, std::string_view key) {
    auto v = obj.get(key);
    return v.is_str() ? std::string(v.as_str()) : std::string{};
}

/// Read an optional string field.
[[nodiscard]] inline std::optional<std::string> read_optional_string(
    cc::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_str()) return std::string(v.as_str());
    return std::nullopt;
}

/// Read an optional int field.
[[nodiscard]] inline std::optional<int64_t> read_optional_int(
    cc::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_num()) return v.as_int();
    return std::nullopt;
}

/// Read an optional double field (yyjson stores 30.0 as a real, not an int;
/// read_optional_int would return 0 for it).
[[nodiscard]] inline std::optional<double> read_optional_double(
    cc::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_num()) return v.as_double();
    return std::nullopt;
}

/// Read an optional bool field.
[[nodiscard]] inline std::optional<bool> read_optional_bool(
    cc::utils::json::JsonVal obj, std::string_view key
) {
    auto v = obj.get(key);
    if (v.is_bool()) return v.as_bool();
    return std::nullopt;
}

/// Read a string vector field.
[[nodiscard]] inline std::vector<std::string> read_string_vec(
    cc::utils::json::JsonVal obj, std::string_view key
) {
    std::vector<std::string> out;
    auto arr = obj.get(key);
    if (arr.is_arr()) {
        arr.iter([&](cc::utils::json::JsonVal el) {
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

[[nodiscard]] inline std::string SlashCommand_to_json(const SlashCommand& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("name", doc.string(v.name));
    o.add("description", doc.string(v.description));
    o.add("argument_hint", doc.string(v.argument_hint));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SlashCommand, std::string> SlashCommand_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    SlashCommand v;
    v.name = detail_serde::read_string(o, "name");
    v.description = detail_serde::read_string(o, "description");
    v.argument_hint = detail_serde::read_string(o, "argument_hint");
    return v;
}
inline std::string to_json(const SlashCommand& v) { return SlashCommand_to_json(v); }
inline std::expected<SlashCommand, std::string> from_json(std::string_view raw, SlashCommand*) {
    return SlashCommand_from_json(raw);
}

[[nodiscard]] inline std::string AgentInfo_to_json(const AgentInfo& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("name", doc.string(v.name));
    o.add("description", doc.string(v.description));
    if (v.model.has_value()) o.add("model", doc.string(*v.model));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<AgentInfo, std::string> AgentInfo_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    AgentInfo v;
    v.name = detail_serde::read_string(o, "name");
    v.description = detail_serde::read_string(o, "description");
    v.model = detail_serde::read_optional_string(o, "model");
    return v;
}
inline std::string to_json(const AgentInfo& v) { return AgentInfo_to_json(v); }
inline std::expected<AgentInfo, std::string> from_json(std::string_view raw, AgentInfo*) {
    return AgentInfo_from_json(raw);
}

[[nodiscard]] inline std::string ModelInfo_to_json(const ModelInfo& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("value", doc.string(v.value));
    o.add("display_name", doc.string(v.display_name));
    o.add("description", doc.string(v.description));
    if (v.supports_effort.has_value()) o.add("supports_effort", doc.boolean(*v.supports_effort));
    if (v.supported_effort_levels.has_value()) {
        auto arr = doc.array();
        for (const auto& s : *v.supported_effort_levels) arr.append(doc.string(s));
        o.add("supported_effort_levels", std::move(arr));
    }
    if (v.supports_adaptive_thinking.has_value())
        o.add("supports_adaptive_thinking", doc.boolean(*v.supports_adaptive_thinking));
    if (v.supports_fast_mode.has_value())
        o.add("supports_fast_mode", doc.boolean(*v.supports_fast_mode));
    if (v.supports_auto_mode.has_value())
        o.add("supports_auto_mode", doc.boolean(*v.supports_auto_mode));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ModelInfo, std::string> ModelInfo_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    ModelInfo v;
    v.value = detail_serde::read_string(o, "value");
    v.display_name = detail_serde::read_string(o, "display_name");
    v.description = detail_serde::read_string(o, "description");
    v.supports_effort = detail_serde::read_optional_bool(o, "supports_effort");
    v.supported_effort_levels = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("supported_effort_levels");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](JsonVal el) { if (el.is_str()) out.emplace_back(el.as_str()); });
        return out;
    }();
    v.supports_adaptive_thinking = detail_serde::read_optional_bool(o, "supports_adaptive_thinking");
    v.supports_fast_mode = detail_serde::read_optional_bool(o, "supports_fast_mode");
    v.supports_auto_mode = detail_serde::read_optional_bool(o, "supports_auto_mode");
    return v;
}
inline std::string to_json(const ModelInfo& v) { return ModelInfo_to_json(v); }
inline std::expected<ModelInfo, std::string> from_json(std::string_view raw, ModelInfo*) {
    return ModelInfo_from_json(raw);
}

[[nodiscard]] inline std::string AccountInfo_to_json(const AccountInfo& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    if (v.email.has_value()) o.add("email", doc.string(*v.email));
    if (v.organization.has_value()) o.add("organization", doc.string(*v.organization));
    if (v.subscription_type.has_value()) o.add("subscription_type", doc.string(*v.subscription_type));
    if (v.token_source.has_value()) o.add("token_source", doc.string(*v.token_source));
    if (v.api_key_source.has_value()) o.add("api_key_source", doc.string(*v.api_key_source));
    if (v.api_provider.has_value()) o.add("api_provider", doc.string(*v.api_provider));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<AccountInfo, std::string> AccountInfo_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    AccountInfo v;
    v.email = detail_serde::read_optional_string(o, "email");
    v.organization = detail_serde::read_optional_string(o, "organization");
    v.subscription_type = detail_serde::read_optional_string(o, "subscription_type");
    v.token_source = detail_serde::read_optional_string(o, "token_source");
    v.api_key_source = detail_serde::read_optional_string(o, "api_key_source");
    v.api_provider = detail_serde::read_optional_string(o, "api_provider");
    return v;
}
inline std::string to_json(const AccountInfo& v) { return AccountInfo_to_json(v); }
inline std::expected<AccountInfo, std::string> from_json(std::string_view raw, AccountInfo*) {
    return AccountInfo_from_json(raw);
}

[[nodiscard]] inline std::string ModelUsage_to_json(const ModelUsage& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("input_tokens", doc.number(static_cast<int64_t>(v.input_tokens)));
    o.add("output_tokens", doc.number(static_cast<int64_t>(v.output_tokens)));
    o.add("cache_read_input_tokens", doc.number(static_cast<int64_t>(v.cache_read_input_tokens)));
    o.add("cache_creation_input_tokens", doc.number(static_cast<int64_t>(v.cache_creation_input_tokens)));
    o.add("web_search_requests", doc.number(static_cast<int64_t>(v.web_search_requests)));
    o.add("cost_usd", doc.number(v.cost_usd));
    o.add("context_window", doc.number(static_cast<int64_t>(v.context_window)));
    o.add("max_output_tokens", doc.number(static_cast<int64_t>(v.max_output_tokens)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ModelUsage, std::string> ModelUsage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    ModelUsage v;
    v.input_tokens = static_cast<int>(o.get("input_tokens").as_int());
    v.output_tokens = static_cast<int>(o.get("output_tokens").as_int());
    v.cache_read_input_tokens = static_cast<int>(o.get("cache_read_input_tokens").as_int());
    v.cache_creation_input_tokens = static_cast<int>(o.get("cache_creation_input_tokens").as_int());
    v.web_search_requests = static_cast<int>(o.get("web_search_requests").as_int());
    v.cost_usd = o.get("cost_usd").as_double();
    v.context_window = static_cast<int>(o.get("context_window").as_int());
    v.max_output_tokens = static_cast<int>(o.get("max_output_tokens").as_int());
    return v;
}
inline std::string to_json(const ModelUsage& v) { return ModelUsage_to_json(v); }
inline std::expected<ModelUsage, std::string> from_json(std::string_view raw, ModelUsage*) {
    return ModelUsage_from_json(raw);
}

// ============================================================================
// Ser/de — control request subtypes
// ============================================================================

// ── ControlInitializeRequest ────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlInitializeRequest_to_json(const ControlInitializeRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("initialize"));
    if (v.hooks.has_value()) {
        auto hooks = doc.object();
        for (const auto& [k, matchers] : *v.hooks) {
            auto arr = doc.array();
            for (const auto& m : matchers) {
                auto mo = doc.object();
                if (m.matcher.has_value()) mo.add("matcher", doc.string(*m.matcher));
                auto ids = doc.array();
                for (const auto& id : m.hook_callback_ids) ids.append(doc.string(id));
                mo.add("hook_callback_ids", std::move(ids));
                if (m.timeout.has_value()) mo.add("timeout", doc.number(*m.timeout));
                arr.append(std::move(mo));
            }
            hooks.add(k.c_str(), std::move(arr));
        }
        o.add("hooks", std::move(hooks));
    }
    if (v.sdk_mcp_servers.has_value()) {
        auto arr = doc.array();
        for (const auto& s : *v.sdk_mcp_servers) arr.append(doc.string(s));
        o.add("sdk_mcp_servers", std::move(arr));
    }
    if (v.system_prompt.has_value()) o.add("system_prompt", doc.string(*v.system_prompt));
    if (v.append_system_prompt.has_value())
        o.add("append_system_prompt", doc.string(*v.append_system_prompt));
    if (v.prompt_suggestions.has_value())
        o.add("prompt_suggestions", doc.boolean(*v.prompt_suggestions));
    if (v.agent_progress_summaries.has_value())
        o.add("agent_progress_summaries", doc.boolean(*v.agent_progress_summaries));
    doc.set_root(std::move(o));
    return doc.to_string();
}

// ── ControlInterruptRequest ─────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlInterruptRequest_to_json(const ControlInterruptRequest&) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("interrupt"));
    doc.set_root(std::move(o));
    return doc.to_string();
}

// ── ControlPermissionRequest (golden-gate) ──────────────────────────────────

[[nodiscard]] inline std::string ControlPermissionRequest_to_json(const ControlPermissionRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("can_use_tool"));
    o.add("tool_name", doc.string(v.tool_name));
    // input is the canonical (parsed + re-serialized) JSON; embed verbatim.
    o.add("input", doc.raw_json(v.input_json.empty() ? "{}" : v.input_json));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    if (v.description.has_value()) o.add("description", doc.string(*v.description));
    if (v.blocked_path.has_value()) o.add("blocked_path", doc.string(*v.blocked_path));
    if (v.decision_reason.has_value()) o.add("decision_reason", doc.string(*v.decision_reason));
    if (v.title.has_value()) o.add("title", doc.string(*v.title));
    if (v.display_name.has_value()) o.add("display_name", doc.string(*v.display_name));
    if (v.agent_id.has_value()) o.add("agent_id", doc.string(*v.agent_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlPermissionRequest, std::string>
ControlPermissionRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlPermissionRequest v;
    v.tool_name = detail_serde::read_string(inner, "tool_name");
    auto input = inner.get("input");
    v.input_json = input.valid() ? input.to_string() : "{}";
    v.tool_use_id = detail_serde::read_string(inner, "tool_use_id");
    v.description = detail_serde::read_optional_string(inner, "description");
    v.blocked_path = detail_serde::read_optional_string(inner, "blocked_path");
    v.decision_reason = detail_serde::read_optional_string(inner, "decision_reason");
    v.title = detail_serde::read_optional_string(inner, "title");
    v.display_name = detail_serde::read_optional_string(inner, "display_name");
    v.agent_id = detail_serde::read_optional_string(inner, "agent_id");
    return v;
}

// ── ControlSetPermissionModeRequest (golden-gate) ───────────────────────────

[[nodiscard]] inline std::string ControlSetPermissionModeRequest_to_json(
    const ControlSetPermissionModeRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_permission_mode"));
    o.add("mode", doc.string(std::string(detail_serde::permission_mode_to_str(v.mode))));
    if (v.ultraplan.has_value()) o.add("ultraplan", doc.boolean(*v.ultraplan));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlSetPermissionModeRequest, std::string>
ControlSetPermissionModeRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlSetPermissionModeRequest v;
    auto mode_str = detail_serde::read_string(inner, "mode");
    if (auto m = detail_serde::permission_mode_from_str(mode_str)) {
        v.mode = *m;
    }
    v.ultraplan = detail_serde::read_optional_bool(inner, "ultraplan");
    return v;
}

// ── ControlSetModelRequest (golden-gate) ────────────────────────────────────

[[nodiscard]] inline std::string ControlSetModelRequest_to_json(const ControlSetModelRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_model"));
    if (v.model.has_value()) o.add("model", doc.string(*v.model));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlSetModelRequest, std::string>
ControlSetModelRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlSetModelRequest v;
    v.model = detail_serde::read_optional_string(inner, "model");
    return v;
}

// ── ControlSetMaxThinkingTokensRequest (golden-gate) ────────────────────────

[[nodiscard]] inline std::string ControlSetMaxThinkingTokensRequest_to_json(
    const ControlSetMaxThinkingTokensRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_max_thinking_tokens"));
    if (v.max_thinking_tokens.has_value())
        o.add("max_thinking_tokens", doc.number(static_cast<int64_t>(*v.max_thinking_tokens)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlSetMaxThinkingTokensRequest, std::string>
ControlSetMaxThinkingTokensRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlSetMaxThinkingTokensRequest v;
    if (auto t = detail_serde::read_optional_int(inner, "max_thinking_tokens")) {
        v.max_thinking_tokens = static_cast<int>(*t);
    }
    return v;
}

// ── ControlRawRequest (unknown subtypes) ────────────────────────────────────

[[nodiscard]] inline std::string ControlRawRequest_to_json(const ControlRawRequest& v) {
    // The raw JSON already includes the subtype field.
    return v.raw_json.empty() ? "{}" : v.raw_json;
}

// ── McpServerConfig variant ─────────────────────────────────────────────────

[[nodiscard]] inline std::string McpStdioServerConfig_to_json(const McpStdioServerConfig& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("stdio"));
    o.add("command", doc.string(v.command));
    if (v.args) {
        auto arr = doc.array();
        for (const auto& a : *v.args) arr.append(doc.string(a));
        o.add("args", std::move(arr));
    }
    if (v.env) {
        auto e = doc.object();
        for (const auto& [k, val] : *v.env) e.add(k.c_str(), doc.string(val));
        o.add("env", std::move(e));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<McpStdioServerConfig, std::string>
McpStdioServerConfig_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    McpStdioServerConfig v;
    v.command = detail_serde::read_string(o, "command");
    if (auto arr = o.get("args"); arr.is_arr()) {
        std::vector<std::string> args;
        arr.iter([&](JsonVal el) { if (el.is_str()) args.emplace_back(el.as_str()); });
        v.args = std::move(args);
    }
    if (auto m = o.get("env"); m.is_obj()) {
        std::unordered_map<std::string, std::string> env;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                env[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.env = std::move(env);
    }
    return v;
}

[[nodiscard]] inline std::string McpSSEServerConfig_to_json(const McpSSEServerConfig& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("sse"));
    o.add("url", doc.string(v.url));
    if (v.headers) {
        auto h = doc.object();
        for (const auto& [k, val] : *v.headers) h.add(k.c_str(), doc.string(val));
        o.add("headers", std::move(h));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<McpSSEServerConfig, std::string>
McpSSEServerConfig_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    McpSSEServerConfig v;
    v.url = detail_serde::read_string(o, "url");
    if (auto m = o.get("headers"); m.is_obj()) {
        std::unordered_map<std::string, std::string> headers;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                headers[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.headers = std::move(headers);
    }
    return v;
}

[[nodiscard]] inline std::string McpHttpServerConfig_to_json(const McpHttpServerConfig& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("http"));
    o.add("url", doc.string(v.url));
    if (v.headers) {
        auto h = doc.object();
        for (const auto& [k, val] : *v.headers) h.add(k.c_str(), doc.string(val));
        o.add("headers", std::move(h));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<McpHttpServerConfig, std::string>
McpHttpServerConfig_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    McpHttpServerConfig v;
    v.url = detail_serde::read_string(o, "url");
    if (auto m = o.get("headers"); m.is_obj()) {
        std::unordered_map<std::string, std::string> headers;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                headers[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.headers = std::move(headers);
    }
    return v;
}

[[nodiscard]] inline std::string McpSdkServerConfig_to_json(const McpSdkServerConfig& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("sdk"));
    o.add("name", doc.string(v.name));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<McpSdkServerConfig, std::string>
McpSdkServerConfig_from_json(cc::utils::json::JsonVal o) {
    McpSdkServerConfig v;
    v.name = detail_serde::read_string(o, "name");
    return v;
}

[[nodiscard]] inline std::string McpServerConfig_to_json(const McpServerConfig& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, McpStdioServerConfig>)
                return McpStdioServerConfig_to_json(alt);
            else if constexpr (std::is_same_v<T, McpSSEServerConfig>)
                return McpSSEServerConfig_to_json(alt);
            else if constexpr (std::is_same_v<T, McpHttpServerConfig>)
                return McpHttpServerConfig_to_json(alt);
            else
                return McpSdkServerConfig_to_json(alt);
        },
        v);
}
[[nodiscard]] inline std::expected<McpServerConfig, std::string>
McpServerConfig_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    if (!o.is_obj()) return std::unexpected("McpServerConfig: expected object");
    const auto type = detail_serde::read_string(o, "type");
    if (type == "stdio") {
        auto r = McpStdioServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "sse") {
        auto r = McpSSEServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "http") {
        auto r = McpHttpServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "sdk") {
        auto r = McpSdkServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    return std::unexpected("McpServerConfig: unknown transport type: " + type);
}
[[nodiscard]] inline std::expected<McpServerConfig, std::string>
McpServerConfig_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return McpServerConfig_from_json(parsed->root());
}

// ── ControlInitializeRequest (full parse) ───────────────────────────────────

[[nodiscard]] inline std::expected<ControlInitializeRequest, std::string>
ControlInitializeRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlInitializeRequest v;
    if (auto hooks = inner.get("hooks"); hooks.is_obj()) {
        std::unordered_map<std::string, std::vector<HookCallbackMatcher>> out;
        hooks.iter_obj([&](JsonVal key, JsonVal val) {
            if (!key.is_str() || !val.is_arr()) return;
            std::vector<HookCallbackMatcher> matchers;
            val.iter([&](JsonVal el) {
                if (!el.is_obj()) return;
                HookCallbackMatcher m;
                m.matcher = detail_serde::read_optional_string(el, "matcher");
                m.hook_callback_ids = detail_serde::read_string_vec(el, "hook_callback_ids");
                if (auto t = detail_serde::read_optional_double(el, "timeout"))
                    m.timeout = *t;
                matchers.push_back(std::move(m));
            });
            out[std::string(key.as_str())] = std::move(matchers);
        });
        v.hooks = std::move(out);
    }
    if (auto arr = inner.get("sdk_mcp_servers"); arr.is_arr()) {
        std::vector<std::string> servers;
        arr.iter([&](JsonVal el) { if (el.is_str()) servers.emplace_back(el.as_str()); });
        v.sdk_mcp_servers = std::move(servers);
    }
    v.system_prompt = detail_serde::read_optional_string(inner, "system_prompt");
    v.append_system_prompt = detail_serde::read_optional_string(inner, "append_system_prompt");
    v.prompt_suggestions = detail_serde::read_optional_bool(inner, "prompt_suggestions");
    v.agent_progress_summaries =
        detail_serde::read_optional_bool(inner, "agent_progress_summaries");
    return v;
}

// ── Remaining control request subtypes ──────────────────────────────────────

[[nodiscard]] inline std::string ControlMcpStatusRequest_to_json(
    const ControlMcpStatusRequest&
) {
    return R"({"subtype":"mcp_status"})";
}

[[nodiscard]] inline std::string ControlGetContextUsageRequest_to_json(
    const ControlGetContextUsageRequest&
) {
    return R"({"subtype":"get_context_usage"})";
}

[[nodiscard]] inline std::string ControlRewindFilesRequest_to_json(
    const ControlRewindFilesRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("rewind_files"));
    o.add("user_message_id", doc.string(v.user_message_id));
    if (v.dry_run) o.add("dry_run", doc.boolean(*v.dry_run));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlRewindFilesRequest, std::string>
ControlRewindFilesRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlRewindFilesRequest v;
    v.user_message_id = detail_serde::read_string(inner, "user_message_id");
    v.dry_run = detail_serde::read_optional_bool(inner, "dry_run");
    return v;
}

[[nodiscard]] inline std::string ControlCancelAsyncMessageRequest_to_json(
    const ControlCancelAsyncMessageRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("cancel_async_message"));
    o.add("message_uuid", doc.string(v.message_uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlCancelAsyncMessageRequest, std::string>
ControlCancelAsyncMessageRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlCancelAsyncMessageRequest v;
    v.message_uuid = detail_serde::read_string(inner, "message_uuid");
    return v;
}

[[nodiscard]] inline std::string ControlSeedReadStateRequest_to_json(
    const ControlSeedReadStateRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("seed_read_state"));
    o.add("path", doc.string(v.path));
    o.add("mtime", doc.number(v.mtime));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlSeedReadStateRequest, std::string>
ControlSeedReadStateRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlSeedReadStateRequest v;
    v.path = detail_serde::read_string(inner, "path");
    if (auto m = inner.get("mtime"); m.is_num()) v.mtime = m.as_double();
    return v;
}

[[nodiscard]] inline std::string HookCallbackRequest_to_json(const HookCallbackRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("hook_callback"));
    o.add("callback_id", doc.string(v.callback_id));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    auto input = doc.object();
    for (const auto& [k, val] : v.input) input.add(k.c_str(), doc.string(val));
    o.add("input", std::move(input));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<HookCallbackRequest, std::string>
HookCallbackRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    HookCallbackRequest v;
    v.callback_id = detail_serde::read_string(inner, "callback_id");
    v.tool_use_id = detail_serde::read_string(inner, "tool_use_id");
    if (auto m = inner.get("input"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                v.input[std::string(k.as_str())] = std::string(val.as_str());
        });
    }
    return v;
}

[[nodiscard]] inline std::string ControlMcpMessageRequest_to_json(
    const ControlMcpMessageRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_message"));
    o.add("server_name", doc.string(v.server_name));
    o.add("message", doc.string(v.message));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlMcpMessageRequest, std::string>
ControlMcpMessageRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlMcpMessageRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    v.message = detail_serde::read_string(inner, "message");
    return v;
}

[[nodiscard]] inline std::string ControlMcpSetServersRequest_to_json(
    const ControlMcpSetServersRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_set_servers"));
    auto servers = doc.object();
    for (const auto& [k, cfg] : v.servers)
        servers.add(k.c_str(), doc.raw_json(McpServerConfig_to_json(cfg)));
    o.add("servers", std::move(servers));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlMcpSetServersRequest, std::string>
ControlMcpSetServersRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlMcpSetServersRequest v;
    if (auto m = inner.get("servers"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto cfg = McpServerConfig_from_json(val);
            if (cfg) v.servers[std::string(k.as_str())] = std::move(*cfg);
        });
    }
    return v;
}

[[nodiscard]] inline std::string ControlReloadPluginsRequest_to_json(
    const ControlReloadPluginsRequest&
) {
    return R"({"subtype":"reload_plugins"})";
}

[[nodiscard]] inline std::string ControlMcpReconnectRequest_to_json(
    const ControlMcpReconnectRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_reconnect"));
    o.add("server_name", doc.string(v.server_name));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlMcpReconnectRequest, std::string>
ControlMcpReconnectRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlMcpReconnectRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    return v;
}

[[nodiscard]] inline std::string ControlMcpToggleRequest_to_json(
    const ControlMcpToggleRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_toggle"));
    o.add("server_name", doc.string(v.server_name));
    o.add("enabled", doc.boolean(v.enabled));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlMcpToggleRequest, std::string>
ControlMcpToggleRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlMcpToggleRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    if (auto e = inner.get("enabled"); e.is_bool()) v.enabled = e.as_bool();
    return v;
}

[[nodiscard]] inline std::string ControlStopTaskRequest_to_json(
    const ControlStopTaskRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("stop_task"));
    o.add("task_id", doc.string(v.task_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlStopTaskRequest, std::string>
ControlStopTaskRequest_from_json(cc::utils::json::JsonVal inner) {
    ControlStopTaskRequest v;
    v.task_id = detail_serde::read_string(inner, "task_id");
    return v;
}

[[nodiscard]] inline std::string ControlApplyFlagSettingsRequest_to_json(
    const ControlApplyFlagSettingsRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("apply_flag_settings"));
    auto settings = doc.object();
    for (const auto& [k, val] : v.settings) settings.add(k.c_str(), doc.string(val));
    o.add("settings", std::move(settings));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlApplyFlagSettingsRequest, std::string>
ControlApplyFlagSettingsRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlApplyFlagSettingsRequest v;
    if (auto m = inner.get("settings"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                v.settings[std::string(k.as_str())] = std::string(val.as_str());
        });
    }
    return v;
}

[[nodiscard]] inline std::string ControlGetSettingsRequest_to_json(
    const ControlGetSettingsRequest&
) {
    return R"({"subtype":"get_settings"})";
}

[[nodiscard]] inline std::string ControlElicitationRequest_to_json(
    const ControlElicitationRequest& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("elicitation"));
    o.add("mcp_server_name", doc.string(v.mcp_server_name));
    o.add("message", doc.string(v.message));
    if (v.mode) o.add("mode", doc.string(*v.mode));
    if (v.url) o.add("url", doc.string(*v.url));
    if (v.elicitation_id) o.add("elicitation_id", doc.string(*v.elicitation_id));
    if (v.requested_schema) {
        auto rs = doc.object();
        for (const auto& [k, val] : *v.requested_schema) rs.add(k.c_str(), doc.string(val));
        o.add("requested_schema", std::move(rs));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlElicitationRequest, std::string>
ControlElicitationRequest_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlElicitationRequest v;
    v.mcp_server_name = detail_serde::read_string(inner, "mcp_server_name");
    v.message = detail_serde::read_string(inner, "message");
    v.mode = detail_serde::read_optional_string(inner, "mode");
    v.url = detail_serde::read_optional_string(inner, "url");
    v.elicitation_id = detail_serde::read_optional_string(inner, "elicitation_id");
    if (auto m = inner.get("requested_schema"); m.is_obj()) {
        std::unordered_map<std::string, std::string> rs;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                rs[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.requested_schema = std::move(rs);
    }
    return v;
}

// ── Variant dispatch: ControlRequestInner → JSON ───────────────────────────

[[nodiscard]] inline std::string control_request_inner_to_json(const ControlRequestInner& inner) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlInitializeRequest>)
                return ControlInitializeRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlInterruptRequest>)
                return ControlInterruptRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlPermissionRequest>)
                return ControlPermissionRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetPermissionModeRequest>)
                return ControlSetPermissionModeRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetModelRequest>)
                return ControlSetModelRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetMaxThinkingTokensRequest>)
                return ControlSetMaxThinkingTokensRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpStatusRequest>)
                return ControlMcpStatusRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlGetContextUsageRequest>)
                return ControlGetContextUsageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRewindFilesRequest>)
                return ControlRewindFilesRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlCancelAsyncMessageRequest>)
                return ControlCancelAsyncMessageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSeedReadStateRequest>)
                return ControlSeedReadStateRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, HookCallbackRequest>)
                return HookCallbackRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpMessageRequest>)
                return ControlMcpMessageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpSetServersRequest>)
                return ControlMcpSetServersRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlReloadPluginsRequest>)
                return ControlReloadPluginsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpReconnectRequest>)
                return ControlMcpReconnectRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpToggleRequest>)
                return ControlMcpToggleRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlStopTaskRequest>)
                return ControlStopTaskRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlApplyFlagSettingsRequest>)
                return ControlApplyFlagSettingsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlGetSettingsRequest>)
                return ControlGetSettingsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlElicitationRequest>)
                return ControlElicitationRequest_to_json(alt);
            else
                return ControlRawRequest_to_json(alt);
        },
        inner);
}

/// Extract the subtype string from any ControlRequestInner alternative.
[[nodiscard]] inline std::string control_request_subtype_str(const ControlRequestInner& inner) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlRawRequest>)
                return alt.subtype;
            else
                return std::string(T::subtype);
        },
        inner);
}

/// Parse a ControlRequestInner from a subtype string + the inner JSON object.
/// Unknown subtypes produce a ControlRawRequest (never a parse failure).
[[nodiscard]] inline ControlRequestInner parse_control_request_inner(
    std::string_view subtype,
    cc::utils::json::JsonVal inner
) {
    if (subtype == "initialize") {
        auto r = ControlInitializeRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "interrupt") return ControlInterruptRequest{};
    if (subtype == "can_use_tool") {
        auto r = ControlPermissionRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_permission_mode") {
        auto r = ControlSetPermissionModeRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_model") {
        auto r = ControlSetModelRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_max_thinking_tokens") {
        auto r = ControlSetMaxThinkingTokensRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_status") return ControlMcpStatusRequest{};
    if (subtype == "get_context_usage") return ControlGetContextUsageRequest{};
    if (subtype == "rewind_files") {
        auto r = ControlRewindFilesRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "cancel_async_message") {
        auto r = ControlCancelAsyncMessageRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "seed_read_state") {
        auto r = ControlSeedReadStateRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "hook_callback") {
        auto r = HookCallbackRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_message") {
        auto r = ControlMcpMessageRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_set_servers") {
        auto r = ControlMcpSetServersRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "reload_plugins") return ControlReloadPluginsRequest{};
    if (subtype == "mcp_reconnect") {
        auto r = ControlMcpReconnectRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_toggle") {
        auto r = ControlMcpToggleRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "stop_task") {
        auto r = ControlStopTaskRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "apply_flag_settings") {
        auto r = ControlApplyFlagSettingsRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "get_settings") return ControlGetSettingsRequest{};
    if (subtype == "elicitation") {
        auto r = ControlElicitationRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    // Unrecognized subtype — raw passthrough.
    return ControlRawRequest{std::string(subtype), inner.to_string()};
}

// ============================================================================
// Ser/de — control response subtypes
// ============================================================================

// ── ControlInitializeResponse (golden-gate) ─────────────────────────────────

[[nodiscard]] inline std::string ControlInitializeResponse_to_json(
    const ControlInitializeResponse& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    // commands: always emitted (even when empty)
    auto cmds = doc.array();
    for (const auto& c : v.commands) cmds.append(doc.raw_json(SlashCommand_to_json(c)));
    o.add("commands", std::move(cmds));
    // agents: emitted only when non-empty
    if (!v.agents.empty()) {
        auto agents = doc.array();
        for (const auto& a : v.agents) agents.append(doc.raw_json(AgentInfo_to_json(a)));
        o.add("agents", std::move(agents));
    }
    o.add("output_style", doc.string(v.output_style));
    auto styles = doc.array();
    for (const auto& s : v.available_output_styles) styles.append(doc.string(s));
    o.add("available_output_styles", std::move(styles));
    // models: always emitted (even when empty)
    auto models = doc.array();
    for (const auto& m : v.models) models.append(doc.raw_json(ModelInfo_to_json(m)));
    o.add("models", std::move(models));
    // account: always emitted (even when empty)
    o.add("account", doc.raw_json(AccountInfo_to_json(v.account)));
    if (v.pid.has_value())
        o.add("pid", doc.number(static_cast<int64_t>(*v.pid)));
    if (v.fast_mode_state.has_value())
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlInitializeResponse, std::string>
ControlInitializeResponse_from_json(cc::utils::json::JsonVal inner) {
    using namespace cc::utils::json;
    ControlInitializeResponse v;
    v.commands = [&] {
        std::vector<SlashCommand> out;
        auto arr = inner.get("commands");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = SlashCommand_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    v.agents = [&] {
        std::vector<AgentInfo> out;
        auto arr = inner.get("agents");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = AgentInfo_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    v.output_style = detail_serde::read_string(inner, "output_style");
    v.available_output_styles = detail_serde::read_string_vec(inner, "available_output_styles");
    v.models = [&] {
        std::vector<ModelInfo> out;
        auto arr = inner.get("models");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = ModelInfo_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    if (auto acc = inner.get("account"); acc.is_obj()) {
        if (auto r = AccountInfo_from_json(acc.to_string())) v.account = *r;
    }
    if (auto p = detail_serde::read_optional_int(inner, "pid")) v.pid = static_cast<int>(*p);
    if (auto fms = inner.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    return v;
}
[[nodiscard]] inline std::expected<ControlInitializeResponse, std::string>
ControlInitializeResponse_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlInitializeResponse_from_json(parsed->root());
}

// ============================================================================
// Ser/de — control wrappers (golden-gate)
// ============================================================================

// ── ControlSuccessResponse ──────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlSuccessResponse_to_json(const ControlSuccessResponse& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("success"));
    o.add("request_id", doc.string(v.request_id));
    // response is always present on the wire (empty object when unset).
    o.add("response", doc.raw_json(v.response_json.value_or("{}")));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlSuccessResponse, std::string>
ControlSuccessResponse_from_json(cc::utils::json::JsonVal inner) {
    ControlSuccessResponse v;
    v.request_id = detail_serde::read_string(inner, "request_id");
    auto resp = inner.get("response");
    if (resp.valid() && !resp.is_null()) {
        v.response_json = resp.to_string();
    }
    return v;
}
[[nodiscard]] inline std::expected<ControlSuccessResponse, std::string>
ControlSuccessResponse_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlSuccessResponse_from_json(parsed->root());
}

// ── ControlErrorResponse ────────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlErrorResponse_to_json(const ControlErrorResponse& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("error"));
    o.add("request_id", doc.string(v.request_id));
    o.add("error", doc.string(v.error));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlErrorResponse, std::string>
ControlErrorResponse_from_json(cc::utils::json::JsonVal inner) {
    ControlErrorResponse v;
    v.request_id = detail_serde::read_string(inner, "request_id");
    v.error = detail_serde::read_string(inner, "error");
    return v;
}
[[nodiscard]] inline std::expected<ControlErrorResponse, std::string>
ControlErrorResponse_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlErrorResponse_from_json(parsed->root());
}

// ── ControlRequest envelope ─────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlRequest_to_json(const ControlRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_request"));
    o.add("request_id", doc.string(v.request_id));
    o.add("request", doc.raw_json(control_request_inner_to_json(v.request)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlRequest, std::string> ControlRequest_from_json(
    std::string_view raw
) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    auto type = root.get("type");
    if (!type.is_str() || type.as_str() != std::string_view("control_request")) {
        return std::unexpected("not a control_request");
    }
    ControlRequest req;
    req.request_id = detail_serde::read_string(root, "request_id");
    auto inner = root.get("request");
    if (!inner.is_obj()) return std::unexpected("request: expected object");
    auto subtype = inner.get("subtype");
    if (!subtype.is_str()) return std::unexpected("subtype: expected string");
    req.request = parse_control_request_inner(subtype.as_str(), inner);
    return req;
}
inline std::string to_json(const ControlRequest& v) { return ControlRequest_to_json(v); }
inline std::expected<ControlRequest, std::string> from_json(std::string_view raw, ControlRequest*) {
    return ControlRequest_from_json(raw);
}

// ── ControlResponse envelope ────────────────────────────────────────────────

namespace detail_serde {

[[nodiscard]] inline std::string control_response_inner_to_json(
    const std::variant<ControlSuccessResponse, ControlErrorResponse>& inner
) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlSuccessResponse>)
                return ControlSuccessResponse_to_json(alt);
            else
                return ControlErrorResponse_to_json(alt);
        },
        inner);
}

} // namespace detail_serde

[[nodiscard]] inline std::string ControlResponse_to_json(const ControlResponse& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_response"));
    // session_id: present on bridge→server, absent on server→client.
    if (v.session_id.has_value()) {
        o.add("session_id", doc.string(*v.session_id));
    }
    o.add("response", doc.raw_json(detail_serde::control_response_inner_to_json(v.response)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlResponse, std::string> ControlResponse_from_json(
    std::string_view raw
) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    auto type = root.get("type");
    if (!type.is_str() || type.as_str() != std::string_view("control_response")) {
        return std::unexpected("not a control_response");
    }
    ControlResponse resp;
    resp.session_id = detail_serde::read_optional_string(root, "session_id");
    auto inner = root.get("response");
    if (!inner.is_obj()) return std::unexpected("response: expected object");
    auto subtype = inner.get("subtype");
    if (!subtype.is_str()) return std::unexpected("subtype: expected string");
    if (subtype.as_str() == std::string_view("error")) {
        auto r = ControlErrorResponse_from_json(inner);
        if (!r) return std::unexpected(r.error());
        resp.response = std::move(*r);
    } else {
        auto r = ControlSuccessResponse_from_json(inner);
        if (!r) return std::unexpected(r.error());
        resp.response = std::move(*r);
    }
    return resp;
}
inline std::string to_json(const ControlResponse& v) { return ControlResponse_to_json(v); }
inline std::expected<ControlResponse, std::string> from_json(std::string_view raw, ControlResponse*) {
    return ControlResponse_from_json(raw);
}

// ============================================================================
// Ser/de — SDK stdout wire-message family
// ============================================================================

namespace detail_serde {

[[nodiscard]] inline std::string string_map_to_json(
    const std::unordered_map<std::string, std::string>& m
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    for (const auto& [k, v] : m) o.add(k.c_str(), doc.string(v));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] inline std::unordered_map<std::string, std::string> string_map_from_json(
    cc::utils::json::JsonVal o
) {
    using namespace cc::utils::json;
    std::unordered_map<std::string, std::string> out;
    if (o.is_obj()) {
        o.iter_obj([&](JsonVal k, JsonVal v) {
            if (k.is_str() && v.is_str())
                out[std::string(k.as_str())] = std::string(v.as_str());
        });
    }
    return out;
}

[[nodiscard]] inline std::string int_map_to_json(
    const std::unordered_map<std::string, int>& m
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    for (const auto& [k, v] : m)
        o.add(k.c_str(), doc.number(static_cast<int64_t>(v)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] inline std::unordered_map<std::string, int> int_map_from_json(
    cc::utils::json::JsonVal o
) {
    using namespace cc::utils::json;
    std::unordered_map<std::string, int> out;
    if (o.is_obj()) {
        o.iter_obj([&](JsonVal k, JsonVal v) {
            if (k.is_str() && v.is_num())
                out[std::string(k.as_str())] = static_cast<int>(v.as_int());
        });
    }
    return out;
}

[[nodiscard]] inline std::optional<SDKAssistantMessageError>
sdk_assistant_message_error_from_str(std::string_view s) {
    if (s == "authentication_failed") return SDKAssistantMessageError::AuthenticationFailed;
    if (s == "billing_error") return SDKAssistantMessageError::BillingError;
    if (s == "rate_limit") return SDKAssistantMessageError::RateLimit;
    if (s == "invalid_request") return SDKAssistantMessageError::InvalidRequest;
    if (s == "server_error") return SDKAssistantMessageError::ServerError;
    if (s == "unknown") return SDKAssistantMessageError::Unknown;
    if (s == "max_output_tokens") return SDKAssistantMessageError::MaxOutputTokens;
    return std::nullopt;
}

[[nodiscard]] inline std::optional<ResultErrorSubtype>
result_error_subtype_from_str(std::string_view s) {
    if (s == "error_during_execution") return ResultErrorSubtype::ErrorDuringExecution;
    if (s == "error_max_turns") return ResultErrorSubtype::ErrorMaxTurns;
    if (s == "error_max_budget_usd") return ResultErrorSubtype::ErrorMaxBudgetUsd;
    if (s == "error_max_structured_output_retries")
        return ResultErrorSubtype::ErrorMaxStructuredOutputRetries;
    return std::nullopt;
}

} // namespace detail_serde

// ── SDKPermissionDenial ─────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKPermissionDenial_to_json(const SDKPermissionDenial& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("tool_name", doc.string(v.tool_name));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    o.add("tool_input", doc.raw_json(detail_serde::string_map_to_json(v.tool_input)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKPermissionDenial, std::string>
SDKPermissionDenial_from_json(cc::utils::json::JsonVal o) {
    SDKPermissionDenial v;
    v.tool_name = detail_serde::read_string(o, "tool_name");
    v.tool_use_id = detail_serde::read_string(o, "tool_use_id");
    v.tool_input = detail_serde::string_map_from_json(o.get("tool_input"));
    return v;
}

// ── SDKUserMessage ──────────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKUserMessage_to_json(const SDKUserMessage& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("user"));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.is_synthetic) o.add("is_synthetic", doc.boolean(*v.is_synthetic));
    if (v.priority) o.add("priority", doc.string(*v.priority));
    if (v.timestamp) o.add("timestamp", doc.string(*v.timestamp));
    if (v.uuid) o.add("uuid", doc.string(*v.uuid));
    if (v.session_id) o.add("session_id", doc.string(*v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKUserMessage, std::string>
SDKUserMessage_from_json(cc::utils::json::JsonVal o) {
    SDKUserMessage v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.is_synthetic = detail_serde::read_optional_bool(o, "is_synthetic");
    v.priority = detail_serde::read_optional_string(o, "priority");
    v.timestamp = detail_serde::read_optional_string(o, "timestamp");
    v.uuid = detail_serde::read_optional_string(o, "uuid");
    v.session_id = detail_serde::read_optional_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKUserMessage, std::string>
SDKUserMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKUserMessage_from_json(parsed->root());
}

// ── SDKUserMessageReplay ────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKUserMessageReplay_to_json(const SDKUserMessageReplay& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("user"));
    o.add("replay", doc.boolean(true));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.is_synthetic) o.add("is_synthetic", doc.boolean(*v.is_synthetic));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKUserMessageReplay, std::string>
SDKUserMessageReplay_from_json(cc::utils::json::JsonVal o) {
    SDKUserMessageReplay v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.is_synthetic = detail_serde::read_optional_bool(o, "is_synthetic");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKUserMessageReplay, std::string>
SDKUserMessageReplay_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKUserMessageReplay_from_json(parsed->root());
}

// ── SDKAssistantMessage ─────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKAssistantMessage_to_json(const SDKAssistantMessage& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("assistant"));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.error)
        o.add("error",
              doc.string(std::string(detail_serde::sdk_assistant_message_error_to_str(*v.error))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKAssistantMessage, std::string>
SDKAssistantMessage_from_json(cc::utils::json::JsonVal o) {
    SDKAssistantMessage v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    if (auto e = detail_serde::read_optional_string(o, "error"))
        v.error = detail_serde::sdk_assistant_message_error_from_str(*e);
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKAssistantMessage, std::string>
SDKAssistantMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKAssistantMessage_from_json(parsed->root());
}

// ── SDKResultSuccess ────────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKResultSuccess_to_json(const SDKResultSuccess& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("result"));
    o.add("subtype", doc.string("success"));
    o.add("duration_ms", doc.number(v.duration_ms));
    o.add("duration_api_ms", doc.number(v.duration_api_ms));
    o.add("is_error", doc.boolean(v.is_error));
    o.add("num_turns", doc.number(static_cast<int64_t>(v.num_turns)));
    o.add("result", doc.string(v.result));
    if (v.stop_reason) o.add("stop_reason", doc.string(*v.stop_reason));
    o.add("total_cost_usd", doc.number(v.total_cost_usd));
    o.add("usage", doc.raw_json(detail_serde::int_map_to_json(v.usage)));
    auto mu = doc.object();
    for (const auto& [k, u] : v.model_usage)
        mu.add(k.c_str(), doc.raw_json(ModelUsage_to_json(u)));
    o.add("model_usage", std::move(mu));
    if (!v.permission_denials.empty()) {
        auto pd = doc.array();
        for (const auto& d : v.permission_denials)
            pd.append(doc.raw_json(SDKPermissionDenial_to_json(d)));
        o.add("permission_denials", std::move(pd));
    }
    if (v.fast_mode_state)
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKResultSuccess, std::string>
SDKResultSuccess_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    SDKResultSuccess v;
    if (auto d = o.get("duration_ms"); d.is_num()) v.duration_ms = d.as_double();
    if (auto d = o.get("duration_api_ms"); d.is_num()) v.duration_api_ms = d.as_double();
    if (auto e = o.get("is_error"); e.is_bool()) v.is_error = e.as_bool();
    if (auto n = o.get("num_turns"); n.is_num()) v.num_turns = static_cast<int>(n.as_int());
    v.result = detail_serde::read_string(o, "result");
    v.stop_reason = detail_serde::read_optional_string(o, "stop_reason");
    if (auto c = o.get("total_cost_usd"); c.is_num()) v.total_cost_usd = c.as_double();
    v.usage = detail_serde::int_map_from_json(o.get("usage"));
    if (auto mu = o.get("model_usage"); mu.is_obj()) {
        mu.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto u = ModelUsage_from_json(val.to_string());
            if (u) v.model_usage[std::string(k.as_str())] = std::move(*u);
        });
    }
    if (auto pd = o.get("permission_denials"); pd.is_arr()) {
        pd.iter([&](JsonVal el) {
            auto d = SDKPermissionDenial_from_json(el);
            if (d) v.permission_denials.push_back(std::move(*d));
        });
    }
    if (auto fms = o.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKResultSuccess, std::string>
SDKResultSuccess_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKResultSuccess_from_json(parsed->root());
}

// ── SDKResultError ──────────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKResultError_to_json(const SDKResultError& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("result"));
    o.add("subtype",
          doc.string(std::string(detail_serde::result_error_subtype_to_str(v.subtype))));
    o.add("duration_ms", doc.number(v.duration_ms));
    o.add("duration_api_ms", doc.number(v.duration_api_ms));
    o.add("is_error", doc.boolean(v.is_error));
    o.add("num_turns", doc.number(static_cast<int64_t>(v.num_turns)));
    if (v.stop_reason) o.add("stop_reason", doc.string(*v.stop_reason));
    o.add("total_cost_usd", doc.number(v.total_cost_usd));
    o.add("usage", doc.raw_json(detail_serde::int_map_to_json(v.usage)));
    auto mu = doc.object();
    for (const auto& [k, u] : v.model_usage)
        mu.add(k.c_str(), doc.raw_json(ModelUsage_to_json(u)));
    o.add("model_usage", std::move(mu));
    if (!v.permission_denials.empty()) {
        auto pd = doc.array();
        for (const auto& d : v.permission_denials)
            pd.append(doc.raw_json(SDKPermissionDenial_to_json(d)));
        o.add("permission_denials", std::move(pd));
    }
    if (!v.errors.empty()) {
        auto errs = doc.array();
        for (const auto& e : v.errors) errs.append(doc.string(e));
        o.add("errors", std::move(errs));
    }
    if (v.fast_mode_state)
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKResultError, std::string>
SDKResultError_from_json(cc::utils::json::JsonVal o) {
    using namespace cc::utils::json;
    SDKResultError v;
    if (auto st = o.get("subtype"); st.is_str()) {
        if (auto s = detail_serde::result_error_subtype_from_str(st.as_str()))
            v.subtype = *s;
    }
    if (auto d = o.get("duration_ms"); d.is_num()) v.duration_ms = d.as_double();
    if (auto d = o.get("duration_api_ms"); d.is_num()) v.duration_api_ms = d.as_double();
    if (auto e = o.get("is_error"); e.is_bool()) v.is_error = e.as_bool();
    if (auto n = o.get("num_turns"); n.is_num()) v.num_turns = static_cast<int>(n.as_int());
    v.stop_reason = detail_serde::read_optional_string(o, "stop_reason");
    if (auto c = o.get("total_cost_usd"); c.is_num()) v.total_cost_usd = c.as_double();
    v.usage = detail_serde::int_map_from_json(o.get("usage"));
    if (auto mu = o.get("model_usage"); mu.is_obj()) {
        mu.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto u = ModelUsage_from_json(val.to_string());
            if (u) v.model_usage[std::string(k.as_str())] = std::move(*u);
        });
    }
    if (auto pd = o.get("permission_denials"); pd.is_arr()) {
        pd.iter([&](JsonVal el) {
            auto d = SDKPermissionDenial_from_json(el);
            if (d) v.permission_denials.push_back(std::move(*d));
        });
    }
    if (auto errs = o.get("errors"); errs.is_arr()) {
        errs.iter([&](JsonVal el) {
            if (el.is_str()) v.errors.emplace_back(el.as_str());
        });
    }
    if (auto fms = o.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKResultError, std::string>
SDKResultError_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKResultError_from_json(parsed->root());
}

// ── SDKSystemMessage ────────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKSystemMessage_to_json(const SDKSystemMessage& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string(v.subtype));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    if (v.model) o.add("model", doc.string(*v.model));
    if (v.permission_mode)
        o.add("permission_mode",
              doc.string(std::string(detail_serde::permission_mode_to_str(*v.permission_mode))));
    if (v.tools) {
        auto arr = doc.array();
        for (const auto& t : *v.tools) arr.append(doc.string(t));
        o.add("tools", std::move(arr));
    }
    if (v.agents) {
        auto arr = doc.array();
        for (const auto& a : *v.agents) arr.append(doc.string(a));
        o.add("agents", std::move(arr));
    }
    if (v.cwd) o.add("cwd", doc.string(*v.cwd));
    if (v.output_style) o.add("output_style", doc.string(*v.output_style));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKSystemMessage, std::string>
SDKSystemMessage_from_json(cc::utils::json::JsonVal o) {
    SDKSystemMessage v;
    v.subtype = detail_serde::read_string(o, "subtype");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.model = detail_serde::read_optional_string(o, "model");
    if (auto pm = detail_serde::read_optional_string(o, "permission_mode")) {
        if (auto m = detail_serde::permission_mode_from_str(*pm))
            v.permission_mode = *m;
    }
    v.tools = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("tools");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](cc::utils::json::JsonVal el) {
            if (el.is_str()) out.emplace_back(el.as_str());
        });
        return out;
    }();
    v.agents = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("agents");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](cc::utils::json::JsonVal el) {
            if (el.is_str()) out.emplace_back(el.as_str());
        });
        return out;
    }();
    v.cwd = detail_serde::read_optional_string(o, "cwd");
    v.output_style = detail_serde::read_optional_string(o, "output_style");
    return v;
}
[[nodiscard]] inline std::expected<SDKSystemMessage, std::string>
SDKSystemMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKSystemMessage_from_json(parsed->root());
}

// ── SDKPartialAssistantMessage ──────────────────────────────────────────────

[[nodiscard]] inline std::string SDKPartialAssistantMessage_to_json(
    const SDKPartialAssistantMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("stream_event"));
    o.add("event", doc.raw_json(v.event_json.empty() ? "{}" : v.event_json));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKPartialAssistantMessage, std::string>
SDKPartialAssistantMessage_from_json(cc::utils::json::JsonVal o) {
    SDKPartialAssistantMessage v;
    if (auto e = o.get("event"); e.valid()) v.event_json = e.to_string();
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKPartialAssistantMessage, std::string>
SDKPartialAssistantMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKPartialAssistantMessage_from_json(parsed->root());
}

// ── SDKCompactBoundaryMessage ───────────────────────────────────────────────

[[nodiscard]] inline std::string SDKCompactBoundaryMessage_to_json(
    const SDKCompactBoundaryMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("compact_boundary"));
    o.add("trigger", doc.string(v.trigger));
    o.add("pre_tokens", doc.number(static_cast<int64_t>(v.pre_tokens)));
    if (v.preserved_segment) {
        auto ps = doc.object();
        ps.add("head_uuid", doc.string(v.preserved_segment->head_uuid));
        ps.add("anchor_uuid", doc.string(v.preserved_segment->anchor_uuid));
        ps.add("tail_uuid", doc.string(v.preserved_segment->tail_uuid));
        o.add("preserved_segment", std::move(ps));
    }
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKCompactBoundaryMessage, std::string>
SDKCompactBoundaryMessage_from_json(cc::utils::json::JsonVal o) {
    SDKCompactBoundaryMessage v;
    v.trigger = detail_serde::read_string(o, "trigger");
    if (auto pt = o.get("pre_tokens"); pt.is_num())
        v.pre_tokens = static_cast<int>(pt.as_int());
    if (auto ps = o.get("preserved_segment"); ps.is_obj()) {
        SDKCompactBoundaryMessage::PreservedSegment seg;
        seg.head_uuid = detail_serde::read_string(ps, "head_uuid");
        seg.anchor_uuid = detail_serde::read_string(ps, "anchor_uuid");
        seg.tail_uuid = detail_serde::read_string(ps, "tail_uuid");
        v.preserved_segment = std::move(seg);
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKCompactBoundaryMessage, std::string>
SDKCompactBoundaryMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKCompactBoundaryMessage_from_json(parsed->root());
}

// ── SDKStatusMessage ────────────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKStatusMessage_to_json(const SDKStatusMessage& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("status"));
    if (v.status) o.add("status", doc.string(*v.status));
    if (v.permission_mode)
        o.add("permission_mode",
              doc.string(std::string(detail_serde::permission_mode_to_str(*v.permission_mode))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKStatusMessage, std::string>
SDKStatusMessage_from_json(cc::utils::json::JsonVal o) {
    SDKStatusMessage v;
    v.status = detail_serde::read_optional_string(o, "status");
    if (auto pm = detail_serde::read_optional_string(o, "permission_mode")) {
        if (auto m = detail_serde::permission_mode_from_str(*pm))
            v.permission_mode = *m;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKStatusMessage, std::string>
SDKStatusMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStatusMessage_from_json(parsed->root());
}

// ── SDKToolProgressMessage ──────────────────────────────────────────────────

[[nodiscard]] inline std::string SDKToolProgressMessage_to_json(
    const SDKToolProgressMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("tool_progress"));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    o.add("tool_name", doc.string(v.tool_name));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    o.add("elapsed_time_seconds", doc.number(v.elapsed_time_seconds));
    if (v.task_id) o.add("task_id", doc.string(*v.task_id));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKToolProgressMessage, std::string>
SDKToolProgressMessage_from_json(cc::utils::json::JsonVal o) {
    SDKToolProgressMessage v;
    v.tool_use_id = detail_serde::read_string(o, "tool_use_id");
    v.tool_name = detail_serde::read_string(o, "tool_name");
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    if (auto e = o.get("elapsed_time_seconds"); e.is_num())
        v.elapsed_time_seconds = e.as_double();
    v.task_id = detail_serde::read_optional_string(o, "task_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKToolProgressMessage, std::string>
SDKToolProgressMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKToolProgressMessage_from_json(parsed->root());
}

// ── SDKPostTurnSummaryMessage ───────────────────────────────────────────────

[[nodiscard]] inline std::string SDKPostTurnSummaryMessage_to_json(
    const SDKPostTurnSummaryMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("post_turn_summary"));
    o.add("summarizes_uuid", doc.string(v.summarizes_uuid));
    o.add("status_category", doc.string(v.status_category));
    o.add("status_detail", doc.string(v.status_detail));
    o.add("is_noteworthy", doc.boolean(v.is_noteworthy));
    o.add("title", doc.string(v.title));
    o.add("description", doc.string(v.description));
    o.add("recent_action", doc.string(v.recent_action));
    o.add("needs_action", doc.string(v.needs_action));
    if (!v.artifact_urls.empty()) {
        auto arr = doc.array();
        for (const auto& u : v.artifact_urls) arr.append(doc.string(u));
        o.add("artifact_urls", std::move(arr));
    }
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKPostTurnSummaryMessage, std::string>
SDKPostTurnSummaryMessage_from_json(cc::utils::json::JsonVal o) {
    SDKPostTurnSummaryMessage v;
    v.summarizes_uuid = detail_serde::read_string(o, "summarizes_uuid");
    v.status_category = detail_serde::read_string(o, "status_category");
    v.status_detail = detail_serde::read_string(o, "status_detail");
    if (auto n = o.get("is_noteworthy"); n.is_bool()) v.is_noteworthy = n.as_bool();
    v.title = detail_serde::read_string(o, "title");
    v.description = detail_serde::read_string(o, "description");
    v.recent_action = detail_serde::read_string(o, "recent_action");
    v.needs_action = detail_serde::read_string(o, "needs_action");
    if (auto arr = o.get("artifact_urls"); arr.is_arr()) {
        arr.iter([&](cc::utils::json::JsonVal el) {
            if (el.is_str()) v.artifact_urls.emplace_back(el.as_str());
        });
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}
[[nodiscard]] inline std::expected<SDKPostTurnSummaryMessage, std::string>
SDKPostTurnSummaryMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKPostTurnSummaryMessage_from_json(parsed->root());
}

// ── SDKStreamlinedTextMessage ───────────────────────────────────────────────

[[nodiscard]] inline std::string SDKStreamlinedTextMessage_to_json(
    const SDKStreamlinedTextMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("streamlined_text"));
    o.add("text", doc.string(v.text));
    o.add("session_id", doc.string(v.session_id));
    o.add("uuid", doc.string(v.uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKStreamlinedTextMessage, std::string>
SDKStreamlinedTextMessage_from_json(cc::utils::json::JsonVal o) {
    SDKStreamlinedTextMessage v;
    v.text = detail_serde::read_string(o, "text");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    return v;
}
[[nodiscard]] inline std::expected<SDKStreamlinedTextMessage, std::string>
SDKStreamlinedTextMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStreamlinedTextMessage_from_json(parsed->root());
}

// ── SDKStreamlinedToolUseSummaryMessage ─────────────────────────────────────

[[nodiscard]] inline std::string SDKStreamlinedToolUseSummaryMessage_to_json(
    const SDKStreamlinedToolUseSummaryMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("streamlined_tool_use_summary"));
    o.add("tool_summary", doc.string(v.tool_summary));
    o.add("session_id", doc.string(v.session_id));
    o.add("uuid", doc.string(v.uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<SDKStreamlinedToolUseSummaryMessage, std::string>
SDKStreamlinedToolUseSummaryMessage_from_json(cc::utils::json::JsonVal o) {
    SDKStreamlinedToolUseSummaryMessage v;
    v.tool_summary = detail_serde::read_string(o, "tool_summary");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    return v;
}
[[nodiscard]] inline std::expected<SDKStreamlinedToolUseSummaryMessage, std::string>
SDKStreamlinedToolUseSummaryMessage_from_json(std::string_view raw) {
    auto parsed = cc::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStreamlinedToolUseSummaryMessage_from_json(parsed->root());
}

// ── SDKMessage variant dispatch ─────────────────────────────────────────────

[[nodiscard]] inline std::string SDKMessage_to_json(const SDKMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKAssistantMessage>)
                return SDKAssistantMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKUserMessage>)
                return SDKUserMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKUserMessageReplay>)
                return SDKUserMessageReplay_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKResultSuccess>)
                return SDKResultSuccess_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKResultError>)
                return SDKResultError_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKSystemMessage>)
                return SDKSystemMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKPartialAssistantMessage>)
                return SDKPartialAssistantMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKCompactBoundaryMessage>)
                return SDKCompactBoundaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStatusMessage>)
                return SDKStatusMessage_to_json(alt);
            else
                return SDKToolProgressMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] inline std::expected<SDKMessage, std::string>
SDKMessage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("SDKMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("SDKMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "user") {
        auto replay = root.get("replay");
        if (replay.is_bool() && replay.as_bool()) {
            auto r = SDKUserMessageReplay_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
        } else {
            auto r = SDKUserMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
        }
        return std::unexpected("SDKMessage: failed to parse user message");
    }
    if (t == "assistant") {
        auto r = SDKAssistantMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "result") {
        auto st = root.get("subtype");
        if (st.is_str() && st.as_str() == std::string_view("success")) {
            auto r = SDKResultSuccess_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        auto r = SDKResultError_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "system") {
        auto st = root.get("subtype");
        const auto subtype = st.is_str() ? std::string(st.as_str()) : std::string{};
        if (subtype == "compact_boundary") {
            auto r = SDKCompactBoundaryMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        if (subtype == "status") {
            auto r = SDKStatusMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        auto r = SDKSystemMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "stream_event") {
        auto r = SDKPartialAssistantMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "tool_progress") {
        auto r = SDKToolProgressMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    return std::unexpected("SDKMessage: unknown type: " + t);
}

// ============================================================================
// Ser/de — envelopes and misc
// ============================================================================

// ── ControlCancelRequest ────────────────────────────────────────────────────

[[nodiscard]] inline std::string ControlCancelRequest_to_json(const ControlCancelRequest& v) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_cancel"));
    o.add("request_id", doc.string(v.request_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<ControlCancelRequest, std::string>
ControlCancelRequest_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    ControlCancelRequest v;
    v.request_id = detail_serde::read_string(root, "request_id");
    return v;
}
inline std::string to_json(const ControlCancelRequest& v) {
    return ControlCancelRequest_to_json(v);
}
inline std::expected<ControlCancelRequest, std::string> from_json(
    std::string_view raw, ControlCancelRequest*
) {
    return ControlCancelRequest_from_json(raw);
}

// ── KeepAliveMessage ────────────────────────────────────────────────────────

[[nodiscard]] inline std::string KeepAliveMessage_to_json(const KeepAliveMessage&) {
    return R"({"type":"keep_alive"})";
}
[[nodiscard]] inline std::expected<KeepAliveMessage, std::string>
KeepAliveMessage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    if (!parsed->root().is_obj()) return std::unexpected("expected object");
    return KeepAliveMessage{};
}
inline std::string to_json(const KeepAliveMessage& v) {
    return KeepAliveMessage_to_json(v);
}
inline std::expected<KeepAliveMessage, std::string> from_json(
    std::string_view raw, KeepAliveMessage*
) {
    return KeepAliveMessage_from_json(raw);
}

// ── UpdateEnvironmentVariablesMessage ───────────────────────────────────────

[[nodiscard]] inline std::string UpdateEnvironmentVariablesMessage_to_json(
    const UpdateEnvironmentVariablesMessage& v
) {
    using namespace cc::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("update_environment_variables"));
    o.add("variables", doc.raw_json(detail_serde::string_map_to_json(v.variables)));
    doc.set_root(std::move(o));
    return doc.to_string();
}
[[nodiscard]] inline std::expected<UpdateEnvironmentVariablesMessage, std::string>
UpdateEnvironmentVariablesMessage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    UpdateEnvironmentVariablesMessage v;
    v.variables = detail_serde::string_map_from_json(root.get("variables"));
    return v;
}
inline std::string to_json(const UpdateEnvironmentVariablesMessage& v) {
    return UpdateEnvironmentVariablesMessage_to_json(v);
}
inline std::expected<UpdateEnvironmentVariablesMessage, std::string> from_json(
    std::string_view raw, UpdateEnvironmentVariablesMessage*
) {
    return UpdateEnvironmentVariablesMessage_from_json(raw);
}

// ── StdoutMessage envelope ──────────────────────────────────────────────────

[[nodiscard]] inline std::string StdoutMessage_to_json(const StdoutMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKMessage>)
                return SDKMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStreamlinedTextMessage>)
                return SDKStreamlinedTextMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStreamlinedToolUseSummaryMessage>)
                return SDKStreamlinedToolUseSummaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKPostTurnSummaryMessage>)
                return SDKPostTurnSummaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlResponse>)
                return ControlResponse_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRequest>)
                return ControlRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlCancelRequest>)
                return ControlCancelRequest_to_json(alt);
            else
                return KeepAliveMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] inline std::expected<StdoutMessage, std::string>
StdoutMessage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("StdoutMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("StdoutMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "streamlined_text") {
        auto r = SDKStreamlinedTextMessage_from_json(root);
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "streamlined_tool_use_summary") {
        auto r = SDKStreamlinedToolUseSummaryMessage_from_json(root);
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_response") {
        auto r = ControlResponse_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_request") {
        auto r = ControlRequest_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_cancel") {
        auto r = ControlCancelRequest_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "keep_alive") {
        return StdoutMessage{KeepAliveMessage{}};
    }
    if (t == "system") {
        // post_turn_summary is a direct StdoutMessage alternative; the rest
        // of the system/* family goes through SDKMessage.
        auto st = root.get("subtype");
        if (st.is_str() && st.as_str() == std::string_view("post_turn_summary")) {
            auto r = SDKPostTurnSummaryMessage_from_json(root);
            if (r) return StdoutMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
    }
    // Everything else (user/assistant/result/system/stream_event/tool_progress)
    // is an SDKMessage.
    auto r = SDKMessage_from_json(raw);
    if (r) return StdoutMessage{std::move(*r)};
    return std::unexpected(r.error());
}

// ── StdinMessage envelope ───────────────────────────────────────────────────

[[nodiscard]] inline std::string StdinMessage_to_json(const StdinMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKUserMessage>)
                return SDKUserMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRequest>)
                return ControlRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlResponse>)
                return ControlResponse_to_json(alt);
            else if constexpr (std::is_same_v<T, KeepAliveMessage>)
                return KeepAliveMessage_to_json(alt);
            else
                return UpdateEnvironmentVariablesMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] inline std::expected<StdinMessage, std::string>
StdinMessage_from_json(std::string_view raw) {
    using namespace cc::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("StdinMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("StdinMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "user") {
        auto r = SDKUserMessage_from_json(root);
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_request") {
        auto r = ControlRequest_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_response") {
        auto r = ControlResponse_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "keep_alive") {
        return StdinMessage{KeepAliveMessage{}};
    }
    if (t == "update_environment_variables") {
        auto r = UpdateEnvironmentVariablesMessage_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    return std::unexpected("StdinMessage: unknown type: " + t);
}

} // namespace cc::server::control
