/// @file test_sdk_serde.cpp
/// @brief Round-trip ser/de tests for every moved control DTO (RFC 0001
///        phase 2, design §4.2).
///
/// For each type T:  T_from_json(T_to_json(x)) round-trips to the same JSON.
/// Covers:
///   - The 21 ControlRequestInner subtypes (through the ControlRequest wrapper)
///   - The response subtypes and the ControlResponse wrapper
///   - The SDK stdout wire-message family (14 types + SDKMessage variant)
///   - The Stdin/Stdout envelopes
///   - The wire-closure types (SlashCommand, ModelInfo, AccountInfo, …)
///   - The McpServerConfig variant
///
/// Comparison is semantic (json_eq — order-independent for objects), not
/// string equality: the DTOs have no operator== and unordered_map fields
/// serialize in non-deterministic key order. Several tests deliberately use
/// multi-key unordered_maps to exercise the semantic comparison.
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

import std;

import loom.server.control_protocol;
import loom.serdes.json;

namespace {

using namespace loom::server::control;

// Recursive semantic JSON comparison (order-independent for objects).
// unordered_map fields serialize in non-deterministic key order, so string
// comparison is unreliable; comparing the parsed values is stable.
bool json_val_eq(loom::utils::json::JsonVal a, loom::utils::json::JsonVal b) {
    using namespace loom::utils::json;
    if (a.is_null() && b.is_null()) return true;
    if (a.is_bool() && b.is_bool()) return a.as_bool() == b.as_bool();
    if (a.is_num() && b.is_num()) return a.as_double() == b.as_double();
    if (a.is_str() && b.is_str()) return a.as_str() == b.as_str();
    if (a.is_arr() && b.is_arr()) {
        std::vector<JsonVal> a_els, b_els;
        a.iter([&](JsonVal el) { a_els.push_back(el); });
        b.iter([&](JsonVal el) { b_els.push_back(el); });
        if (a_els.size() != b_els.size()) return false;
        for (size_t i = 0; i < a_els.size(); ++i)
            if (!json_val_eq(a_els[i], b_els[i])) return false;
        return true;
    }
    if (a.is_obj() && b.is_obj()) {
        std::vector<std::pair<std::string, JsonVal>> a_fields;
        a.iter_obj([&](JsonVal k, JsonVal v) {
            if (k.is_str()) a_fields.emplace_back(std::string(k.as_str()), v);
        });
        size_t b_count = 0;
        b.iter_obj([&](JsonVal, JsonVal) { ++b_count; });
        if (a_fields.size() != b_count) return false;
        for (const auto& [key, val] : a_fields) {
            auto b_val = b.get(key);
            if (!b_val.valid()) return false;
            if (!json_val_eq(val, b_val)) return false;
        }
        return true;
    }
    return false;
}

// Parse two JSON strings and compare semantically.
::testing::AssertionResult json_eq(const std::string& a, const std::string& b) {
    auto pa = loom::utils::json::parse(a);
    if (!pa) return ::testing::AssertionFailure() << "left parse error: " << pa.error().message();
    auto pb = loom::utils::json::parse(b);
    if (!pb) return ::testing::AssertionFailure() << "right parse error: " << pb.error().message();
    if (json_val_eq(pa->root(), pb->root())) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << "\n  left:  " << a << "\n  right: " << b;
}

// Round-trip: serialize → parse → re-serialize → compare semantically.
#define ROUND_TRIP_STR(to_json_fn, from_json_fn, value)                          \
    do {                                                                         \
        auto _json1 = (to_json_fn)(value);                                       \
        auto _parsed = (from_json_fn)(_json1);                                   \
        ASSERT_TRUE(_parsed.has_value()) << _parsed.error();                     \
        auto _json2 = (to_json_fn)(*_parsed);                                    \
        EXPECT_TRUE(json_eq(_json1, _json2));                                    \
    } while (0)

// Round-trip through the ControlRequest envelope (for inner subtypes whose
// _from_json takes JsonVal, not string_view).
#define ROUND_TRIP_REQUEST(subtype_value)                                        \
    do {                                                                         \
        ControlRequest _req;                                                     \
        _req.request_id = "req-001";                                             \
        _req.request = (subtype_value);                                          \
        ROUND_TRIP_STR(ControlRequest_to_json, ControlRequest_from_json, _req);  \
    } while (0)

// Round-trip for a type with string_view _from_json.
#define ROUND_TRIP_TYPE(type, value)                                             \
    ROUND_TRIP_STR(type##_to_json, type##_from_json, value)

// ============================================================================
// Control request subtypes — the 21 subtypes
// ============================================================================

TEST(SdkSerde, InitializeRequest) {
    ControlInitializeRequest v;
    v.system_prompt = "You are a helpful assistant.";
    v.append_system_prompt = "Be concise.";
    v.sdk_mcp_servers = std::vector<std::string>{"mcp-server-1"};
    v.prompt_suggestions = true;
    v.agent_progress_summaries = false;
    v.hooks = std::unordered_map<std::string, std::vector<HookCallbackMatcher>>{
        {"PreToolUse", {{.matcher = "Bash", .hook_callback_ids = {"cb-1"}, .timeout = 30.0}}},
    };
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, InterruptRequest) {
    ROUND_TRIP_REQUEST(ControlInterruptRequest{});
}

TEST(SdkSerde, PermissionRequest) {
    ControlPermissionRequest v;
    v.tool_name = "Bash";
    v.input_json = R"({"command":"ls -la"})";
    v.tool_use_id = "toolu_123";
    v.agent_id = "agent-1";
    v.description = "Run a shell command";
    v.blocked_path = "/etc/passwd";
    v.decision_reason = "Path is blocked";
    v.title = "Bash Permission";
    v.display_name = "Bash";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, SetPermissionModeRequest) {
    ControlSetPermissionModeRequest v;
    v.mode = PermissionMode::AcceptEdits;
    v.ultraplan = true;
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, SetModelRequest) {
    ControlSetModelRequest v;
    v.model = "claude-sonnet-4-20250514";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, SetMaxThinkingTokensRequest) {
    ControlSetMaxThinkingTokensRequest v;
    v.max_thinking_tokens = 16000;
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, McpStatusRequest) {
    ROUND_TRIP_REQUEST(ControlMcpStatusRequest{});
}

TEST(SdkSerde, GetContextUsageRequest) {
    ROUND_TRIP_REQUEST(ControlGetContextUsageRequest{});
}

TEST(SdkSerde, RewindFilesRequest) {
    ControlRewindFilesRequest v;
    v.user_message_id = "msg-abc-123";
    v.dry_run = true;
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, CancelAsyncMessageRequest) {
    ControlCancelAsyncMessageRequest v;
    v.message_uuid = "uuid-xyz-789";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, SeedReadStateRequest) {
    ControlSeedReadStateRequest v;
    v.path = "/home/user/project/src/main.cpp";
    v.mtime = 1727784000.0;
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, HookCallbackRequest) {
    HookCallbackRequest v;
    v.callback_id = "cb-001";
    v.tool_use_id = "toolu_456";
    v.input = {{"key", "value"}};
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, McpMessageRequest) {
    ControlMcpMessageRequest v;
    v.server_name = "my-mcp-server";
    v.message = R"({"jsonrpc":"2.0","method":"tools/list","id":1})";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, McpSetServersRequest) {
    ControlMcpSetServersRequest v;
    McpStdioServerConfig stdio;
    stdio.command = "npx";
    stdio.args = std::vector<std::string>{"-y", "@modelcontextprotocol/server-filesystem"};
    stdio.env = std::unordered_map<std::string, std::string>{{"DEBUG", "1"}};
    v.servers = {{"fs", McpServerConfig{stdio}}};
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, ReloadPluginsRequest) {
    ROUND_TRIP_REQUEST(ControlReloadPluginsRequest{});
}

TEST(SdkSerde, McpReconnectRequest) {
    ControlMcpReconnectRequest v;
    v.server_name = "disconnected-server";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, McpToggleRequest) {
    ControlMcpToggleRequest v;
    v.server_name = "my-server";
    v.enabled = false;
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, StopTaskRequest) {
    ControlStopTaskRequest v;
    v.task_id = "task-001";
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, ApplyFlagSettingsRequest) {
    ControlApplyFlagSettingsRequest v;
    v.settings = {{"model", "claude-sonnet-4-20250514"}};
    ROUND_TRIP_REQUEST(v);
}

TEST(SdkSerde, GetSettingsRequest) {
    ROUND_TRIP_REQUEST(ControlGetSettingsRequest{});
}

TEST(SdkSerde, ElicitationRequest) {
    ControlElicitationRequest v;
    v.mcp_server_name = "my-mcp-server";
    v.message = "Allow access to /tmp?";
    v.mode = "form";
    v.url = "https://example.com/auth";
    v.elicitation_id = "elic-001";
    v.requested_schema = std::unordered_map<std::string, std::string>{{"type", "object"}};
    ROUND_TRIP_REQUEST(v);
}

// ============================================================================
// Control response subtypes
// ============================================================================

TEST(SdkSerde, InitializeResponse) {
    ControlInitializeResponse v;
    v.output_style = "default";
    v.available_output_styles = {"default", "concise"};
    v.account = AccountInfo{
        .email = "user@example.com",
        .organization = "Anthropic",
        .subscription_type = "pro",
        .token_source = "oauth",
        .api_key_source = "user",
        .api_provider = "firstParty",
    };
    v.commands = {SlashCommand{.name = "review", .description = "Review code", .argument_hint = "[pr]"}};
    v.agents = {AgentInfo{.name = "code-reviewer", .description = "Reviews code", .model = "sonnet"}};
    v.models = {ModelInfo{
        .value = "claude-sonnet-4-20250514",
        .display_name = "Claude Sonnet 4",
        .description = "Smart model",
        .supports_effort = true,
        .supported_effort_levels = std::vector<std::string>{"low", "medium", "high"},
        .supports_adaptive_thinking = true,
        .supports_fast_mode = false,
        .supports_auto_mode = true,
    }};
    v.fast_mode_state = FastModeState::On;
    ROUND_TRIP_TYPE(ControlInitializeResponse, v);
}

TEST(SdkSerde, SuccessResponse) {
    ControlSuccessResponse v;
    v.request_id = "req-001";
    v.response_json = R"({"interrupted":true})";
    ROUND_TRIP_TYPE(ControlSuccessResponse, v);
}

TEST(SdkSerde, ErrorResponse) {
    ControlErrorResponse v;
    v.request_id = "req-002";
    v.error = "Permission denied";
    ROUND_TRIP_TYPE(ControlErrorResponse, v);
}

TEST(SdkSerde, ControlResponseSuccess) {
    ControlResponse v;
    v.session_id = "sess-001";
    ControlSuccessResponse succ;
    succ.request_id = "req-001";
    succ.response_json = R"({"behavior":"allow"})";
    v.response = std::move(succ);
    ROUND_TRIP_TYPE(ControlResponse, v);
}

TEST(SdkSerde, ControlResponseError) {
    ControlResponse v;
    v.session_id = "sess-001";
    ControlErrorResponse err;
    err.request_id = "req-002";
    err.error = "Not found";
    v.response = std::move(err);
    ROUND_TRIP_TYPE(ControlResponse, v);
}

// ============================================================================
// SDK stdout wire-message family
// ============================================================================

TEST(SdkSerde, SDKUserMessage) {
    SDKUserMessage v;
    v.message = {{"role", "user"}, {"content", "Hello"}};
    v.parent_tool_use_id = "toolu_parent";
    v.is_synthetic = false;
    v.priority = "now";
    v.timestamp = "2025-09-30T12:00:00Z";
    v.uuid = "msg-uuid-001";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKUserMessage, v);
}

TEST(SdkSerde, SDKUserMessageReplay) {
    SDKUserMessageReplay v;
    v.message = {{"role", "user"}, {"content", "Replay this"}};
    v.parent_tool_use_id = "toolu_parent";
    v.is_synthetic = true;
    v.uuid = "msg-uuid-002";
    v.session_id = "sess-001";
    ROUND_TRIP_STR(SDKUserMessageReplay_to_json, SDKUserMessageReplay_from_json, v);
}

TEST(SdkSerde, SDKAssistantMessage) {
    SDKAssistantMessage v;
    v.message = {{"role", "assistant"}, {"content", "Hello!"}};
    v.parent_tool_use_id = "toolu_parent";
    v.uuid = "msg-uuid-003";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKAssistantMessage, v);
}

TEST(SdkSerde, SDKAssistantMessageWithError) {
    SDKAssistantMessage v;
    v.message = {{"role", "assistant"}};
    v.error = SDKAssistantMessageError::RateLimit;
    v.uuid = "msg-uuid-004";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKAssistantMessage, v);
}

TEST(SdkSerde, SDKResultSuccess) {
    SDKResultSuccess v;
    v.duration_ms = 1500.5;
    v.duration_api_ms = 1200.25;
    v.is_error = false;
    v.num_turns = 3;
    v.result = "Task completed successfully.";
    v.stop_reason = "end_turn";
    v.total_cost_usd = 0.00234;
    // Opaque usage JSON — includes the nested server_tool_use object the live
    // wire emits (a typed int map would drop it).
    v.usage_json = R"({"input_tokens":1500,"output_tokens":300,)"
                   R"("cache_creation_input_tokens":0,"cache_read_input_tokens":0,)"
                   R"("server_tool_use":{"web_search_requests":0}})";
    v.model_usage = {{"claude-sonnet-4-20250514", ModelUsage{
        .input_tokens = 1500,
        .output_tokens = 300,
        .cache_read_input_tokens = 200,
        .cache_creation_input_tokens = 50,
        .web_search_requests = 0,
        .cost_usd = 0.00234,
        .context_window = 200000,
        .max_output_tokens = 8192,
    }}};
    v.permission_denials = {SDKPermissionDenial{
        .tool_name = "Bash",
        .tool_use_id = "toolu_denied",
        .tool_input = {{"command", "rm -rf /"}},
    }};
    v.structured_output = R"({"answer":42})";
    v.fast_mode_state = FastModeState::Cooldown;
    v.uuid = "msg-uuid-005";
    v.session_id = "sess-001";
    // Wire-spelling lock: the result message field is "modelUsage" (camelCase,
    // matching the TS schema and the live emitters) and the ModelUsage
    // sub-fields are camelCase. A round-trip alone cannot catch this drift.
    const auto wire = SDKResultSuccess_to_json(v);
    EXPECT_NE(wire.find("\"modelUsage\""), std::string::npos);
    EXPECT_EQ(wire.find("\"model_usage\""), std::string::npos);
    EXPECT_NE(wire.find("\"inputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"cacheReadInputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"costUSD\""), std::string::npos);
    EXPECT_NE(wire.find("\"structured_output\""), std::string::npos);
    EXPECT_NE(wire.find("\"server_tool_use\""), std::string::npos);
    ROUND_TRIP_TYPE(SDKResultSuccess, v);
}

TEST(SdkSerde, SDKResultError) {
    SDKResultError v;
    v.subtype = ResultErrorSubtype::ErrorMaxTurns;
    v.duration_ms = 5000.0;
    v.duration_api_ms = 4500.0;
    v.is_error = true;
    v.num_turns = 100;
    v.stop_reason = "max_turns";
    v.total_cost_usd = 0.05;
    v.usage_json = R"({"input_tokens":50000,"output_tokens":10000,)"
                   R"("cache_creation_input_tokens":0,"cache_read_input_tokens":0,)"
                   R"("server_tool_use":{"web_search_requests":0}})";
    v.model_usage = {{"claude-sonnet-4-20250514", ModelUsage{
        .input_tokens = 50000,
        .output_tokens = 10000,
        .cache_read_input_tokens = 0,
        .cache_creation_input_tokens = 0,
        .web_search_requests = 0,
        .cost_usd = 0.05,
        .context_window = 200000,
        .max_output_tokens = 8192,
    }}};
    v.errors = {"Max turns exceeded", "Context window full"};
    v.fast_mode_state = FastModeState::Off;
    v.uuid = "msg-uuid-006";
    v.session_id = "sess-001";
    // Wire-spelling lock (see SDKResultSuccess for rationale).
    const auto wire = SDKResultError_to_json(v);
    EXPECT_NE(wire.find("\"modelUsage\""), std::string::npos);
    EXPECT_EQ(wire.find("\"model_usage\""), std::string::npos);
    EXPECT_NE(wire.find("\"maxOutputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"webSearchRequests\""), std::string::npos);
    ROUND_TRIP_TYPE(SDKResultError, v);
}

TEST(SdkSerde, SDKSystemMessage) {
    SDKSystemMessage v;
    v.subtype = "init";
    v.uuid = "msg-uuid-007";
    v.session_id = "sess-001";
    v.model = "claude-sonnet-4-20250514";
    v.permission_mode = PermissionMode::Default;
    v.tools = std::vector<std::string>{"Bash", "Read", "Write"};
    v.agents = std::vector<std::string>{"code-reviewer"};
    v.cwd = "/home/user/project";
    v.output_style = "default";
    ROUND_TRIP_TYPE(SDKSystemMessage, v);
}

TEST(SdkSerde, SDKPartialAssistantMessage) {
    SDKPartialAssistantMessage v;
    v.event_json = R"({"type":"content_block_delta","delta":{"text":"Hello"}})";
    v.parent_tool_use_id = "toolu_parent";
    v.uuid = "msg-uuid-008";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKPartialAssistantMessage, v);
}

TEST(SdkSerde, SDKCompactBoundaryMessage) {
    SDKCompactBoundaryMessage v;
    v.trigger = "manual";
    v.pre_tokens = 150000;
    v.preserved_segment = SDKCompactBoundaryMessage::PreservedSegment{
        .head_uuid = "head-001",
        .anchor_uuid = "anchor-001",
        .tail_uuid = "tail-001",
    };
    v.uuid = "msg-uuid-009";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKCompactBoundaryMessage, v);
}

TEST(SdkSerde, SDKStatusMessage) {
    SDKStatusMessage v;
    v.status = "compacting";
    v.permission_mode = PermissionMode::Plan;
    v.uuid = "msg-uuid-010";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKStatusMessage, v);
}

TEST(SdkSerde, SDKToolProgressMessage) {
    SDKToolProgressMessage v;
    v.tool_use_id = "toolu_001";
    v.tool_name = "Bash";
    v.parent_tool_use_id = "toolu_parent";
    v.elapsed_time_seconds = 5.5;
    v.task_id = "task-001";
    v.uuid = "msg-uuid-011";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKToolProgressMessage, v);
}

TEST(SdkSerde, SDKPostTurnSummaryMessage) {
    SDKPostTurnSummaryMessage v;
    v.summarizes_uuid = "msg-uuid-005";
    v.status_category = "completed";
    v.status_detail = "All tests passed";
    v.is_noteworthy = true;
    v.title = "Test Run";
    v.description = "Ran the full test suite";
    v.recent_action = "ctest -j1";
    v.needs_action = "No action needed";
    v.artifact_urls = {"https://example.com/report.html"};
    v.uuid = "msg-uuid-012";
    v.session_id = "sess-001";
    ROUND_TRIP_TYPE(SDKPostTurnSummaryMessage, v);
}

TEST(SdkSerde, SDKStreamlinedTextMessage) {
    SDKStreamlinedTextMessage v;
    v.text = "This is a streamlined text message.";
    v.session_id = "sess-001";
    v.uuid = "msg-uuid-013";
    ROUND_TRIP_TYPE(SDKStreamlinedTextMessage, v);
}

TEST(SdkSerde, SDKStreamlinedToolUseSummaryMessage) {
    SDKStreamlinedToolUseSummaryMessage v;
    v.tool_summary = "Read 3 files, edited 1";
    v.session_id = "sess-001";
    v.uuid = "msg-uuid-014";
    ROUND_TRIP_TYPE(SDKStreamlinedToolUseSummaryMessage, v);
}

// ============================================================================
// SDKMessage variant dispatch
// ============================================================================

TEST(SdkSerde, SDKMessageVariantUser) {
    SDKUserMessage inner;
    inner.message = {{"role", "user"}};
    inner.uuid = "msg-001";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantAssistant) {
    SDKAssistantMessage inner;
    inner.message = {{"role", "assistant"}};
    inner.uuid = "msg-002";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantResultSuccess) {
    SDKResultSuccess inner;
    inner.result = "Done";
    inner.uuid = "msg-003";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantResultError) {
    SDKResultError inner;
    inner.subtype = ResultErrorSubtype::ErrorDuringExecution;
    inner.uuid = "msg-004";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantSystem) {
    SDKSystemMessage inner;
    inner.subtype = "init";
    inner.uuid = "msg-005";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantStreamEvent) {
    SDKPartialAssistantMessage inner;
    inner.event_json = R"({"type":"ping"})";
    inner.uuid = "msg-006";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantToolProgress) {
    SDKToolProgressMessage inner;
    inner.tool_use_id = "toolu_001";
    inner.tool_name = "Read";
    inner.uuid = "msg-007";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantCompactBoundary) {
    SDKCompactBoundaryMessage inner;
    inner.trigger = "auto";
    inner.pre_tokens = 100000;
    inner.uuid = "msg-008";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

TEST(SdkSerde, SDKMessageVariantStatus) {
    SDKStatusMessage inner;
    inner.status = "compacting";
    inner.uuid = "msg-009";
    inner.session_id = "sess-001";
    SDKMessage v{std::move(inner)};
    ROUND_TRIP_STR(SDKMessage_to_json, SDKMessage_from_json, v);
}

// ============================================================================
// Envelopes — StdoutMessage / StdinMessage
// ============================================================================

TEST(SdkSerde, StdoutMessageSDKMessage) {
    SDKUserMessage inner;
    inner.message = {{"role", "user"}};
    inner.uuid = "msg-100";
    inner.session_id = "sess-001";
    StdoutMessage v{SDKMessage{std::move(inner)}};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageStreamlinedText) {
    SDKStreamlinedTextMessage inner;
    inner.text = "Streamlined output";
    inner.session_id = "sess-001";
    inner.uuid = "msg-101";
    StdoutMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageStreamlinedToolUseSummary) {
    SDKStreamlinedToolUseSummaryMessage inner;
    inner.tool_summary = "Edited 2 files";
    inner.session_id = "sess-001";
    inner.uuid = "msg-102";
    StdoutMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessagePostTurnSummary) {
    SDKPostTurnSummaryMessage inner;
    inner.summarizes_uuid = "msg-005";
    inner.status_category = "completed";
    inner.status_detail = "Done";
    inner.title = "Summary";
    inner.description = "Turn complete";
    inner.recent_action = "None";
    inner.needs_action = "None";
    inner.uuid = "msg-103";
    inner.session_id = "sess-001";
    StdoutMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageControlResponse) {
    ControlResponse inner;
    inner.session_id = "sess-001";
    ControlSuccessResponse succ;
    succ.request_id = "req-001";
    inner.response = std::move(succ);
    StdoutMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageControlRequest) {
    ControlRequest inner;
    inner.request_id = "req-001";
    inner.request = ControlInterruptRequest{};
    StdoutMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageControlCancel) {
    StdoutMessage v{ControlCancelRequest{.request_id = "req-001"}};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdoutMessageKeepAlive) {
    StdoutMessage v{KeepAliveMessage{}};
    ROUND_TRIP_STR(StdoutMessage_to_json, StdoutMessage_from_json, v);
}

TEST(SdkSerde, StdinMessageUser) {
    SDKUserMessage inner;
    inner.message = {{"role", "user"}, {"content", "Hi"}};
    inner.uuid = "msg-200";
    inner.session_id = "sess-001";
    StdinMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdinMessage_to_json, StdinMessage_from_json, v);
}

TEST(SdkSerde, StdinMessageControlRequest) {
    ControlRequest inner;
    inner.request_id = "req-001";
    inner.request = ControlSetModelRequest{.model = "claude-sonnet-4-20250514"};
    StdinMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdinMessage_to_json, StdinMessage_from_json, v);
}

TEST(SdkSerde, StdinMessageControlResponse) {
    ControlResponse inner;
    inner.session_id = "sess-001";
    ControlSuccessResponse succ;
    succ.request_id = "req-001";
    inner.response = std::move(succ);
    StdinMessage v{std::move(inner)};
    ROUND_TRIP_STR(StdinMessage_to_json, StdinMessage_from_json, v);
}

TEST(SdkSerde, StdinMessageKeepAlive) {
    StdinMessage v{KeepAliveMessage{}};
    ROUND_TRIP_STR(StdinMessage_to_json, StdinMessage_from_json, v);
}

TEST(SdkSerde, StdinMessageUpdateEnvironmentVariables) {
    StdinMessage v{UpdateEnvironmentVariablesMessage{
        .variables = {{"LOOM_DEBUG", "1"}},
    }};
    ROUND_TRIP_STR(StdinMessage_to_json, StdinMessage_from_json, v);
}

// ============================================================================
// Wire-closure types
// ============================================================================

TEST(SdkSerde, SlashCommand) {
    SlashCommand v{.name = "review", .description = "Review code", .argument_hint = "[pr]"};
    ROUND_TRIP_TYPE(SlashCommand, v);
}

TEST(SdkSerde, AgentInfo) {
    AgentInfo v{.name = "reviewer", .description = "Code reviewer", .model = "sonnet"};
    ROUND_TRIP_TYPE(AgentInfo, v);
}

TEST(SdkSerde, ModelInfo) {
    ModelInfo v{
        .value = "claude-sonnet-4-20250514",
        .display_name = "Claude Sonnet 4",
        .description = "Smart",
        .supports_effort = true,
        .supported_effort_levels = std::vector<std::string>{"low", "high"},
        .supports_adaptive_thinking = true,
        .supports_fast_mode = false,
        .supports_auto_mode = true,
    };
    ROUND_TRIP_TYPE(ModelInfo, v);
}

TEST(SdkSerde, AccountInfo) {
    AccountInfo v{
        .email = "user@example.com",
        .organization = "Anthropic",
        .subscription_type = "pro",
        .token_source = "oauth",
        .api_key_source = "user",
        .api_provider = "firstParty",
    };
    ROUND_TRIP_TYPE(AccountInfo, v);
}

TEST(SdkSerde, ModelUsage) {
    ModelUsage v{
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_input_tokens = 200,
        .cache_creation_input_tokens = 50,
        .web_search_requests = 2,
        .cost_usd = 0.00123,
        .context_window = 200000,
        .max_output_tokens = 8192,
    };
    // Wire-spelling lock: sub-fields are camelCase per the TS ModelUsageSchema.
    const auto wire = ModelUsage_to_json(v);
    EXPECT_NE(wire.find("\"inputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"outputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"cacheReadInputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"cacheCreationInputTokens\""), std::string::npos);
    EXPECT_NE(wire.find("\"webSearchRequests\""), std::string::npos);
    EXPECT_NE(wire.find("\"costUSD\""), std::string::npos);
    EXPECT_NE(wire.find("\"contextWindow\""), std::string::npos);
    EXPECT_NE(wire.find("\"maxOutputTokens\""), std::string::npos);
    EXPECT_EQ(wire.find("\"input_tokens\""), std::string::npos);
    ROUND_TRIP_TYPE(ModelUsage, v);
}

// ============================================================================
// McpServerConfig variant
// ============================================================================

TEST(SdkSerde, McpStdioServerConfig) {
    McpStdioServerConfig v;
    v.command = "npx";
    v.args = std::vector<std::string>{"-y", "server"};
    v.env = std::unordered_map<std::string, std::string>{{"KEY", "VAL"}};
    ROUND_TRIP_STR(McpServerConfig_to_json, McpServerConfig_from_json, McpServerConfig{v});
}

TEST(SdkSerde, McpSSEServerConfig) {
    McpSSEServerConfig v;
    v.url = "https://example.com/sse";
    v.headers = std::unordered_map<std::string, std::string>{{"Authorization", "Bearer token"}};
    ROUND_TRIP_STR(McpServerConfig_to_json, McpServerConfig_from_json, McpServerConfig{v});
}

TEST(SdkSerde, McpHttpServerConfig) {
    McpHttpServerConfig v;
    v.url = "https://example.com/mcp";
    v.headers = std::unordered_map<std::string, std::string>{{"X-Key", "value"}};
    ROUND_TRIP_STR(McpServerConfig_to_json, McpServerConfig_from_json, McpServerConfig{v});
}

TEST(SdkSerde, McpSdkServerConfig) {
    McpSdkServerConfig v;
    v.name = "in-process-server";
    ROUND_TRIP_STR(McpServerConfig_to_json, McpServerConfig_from_json, McpServerConfig{v});
}

// ============================================================================
// Misc types
// ============================================================================

TEST(SdkSerde, ControlCancelRequest) {
    ControlCancelRequest v{.request_id = "req-cancel-001"};
    ROUND_TRIP_TYPE(ControlCancelRequest, v);
}

TEST(SdkSerde, KeepAliveMessage) {
    KeepAliveMessage v{};
    ROUND_TRIP_TYPE(KeepAliveMessage, v);
}

TEST(SdkSerde, UpdateEnvironmentVariablesMessage) {
    UpdateEnvironmentVariablesMessage v;
    v.variables = {{"PATH", "/usr/bin"}, {"HOME", "/home/user"}};
    ROUND_TRIP_TYPE(UpdateEnvironmentVariablesMessage, v);
}

}  // namespace
