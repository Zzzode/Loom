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
            else if constexpr (std::is_same_v<T, ControlRawRequest>)
                return ControlRawRequest_to_json(alt);
            else
                // Remaining subtypes: serialize subtype + raw passthrough.
                return std::string("{}");
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
    if (subtype == "initialize") return ControlInitializeRequest{};
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

} // namespace cc::server::control
