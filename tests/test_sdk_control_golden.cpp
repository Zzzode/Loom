/// @file test_sdk_control_golden.cpp
/// @brief Golden wire-compatibility gate for the hand-rolled control protocol
///        spoken by cc.server.server_main and cc.bridge.bridge_messaging
///        (RFC 0001 cc_sdk phases 2-3 design, §4.2 S6).
///
/// Before the hand-rolled JSON is replaced by the new
/// cc.server.control_protocol ser/de, this suite freezes the EXACT bytes the
/// live code produces for every spoken control subtype, and the exact parse
/// the live parsers extract from representative wire input. The next step's
/// ser/de must reproduce these fixtures byte-for-byte (the golden gate) —
/// round-trip tests alone cannot detect field-name/shape drift
/// (camelCase vs snake_case, request_id placement, subtype spelling).
///
/// Coverage (every spoken control subtype):
///   server serializers : can_use_tool control_request (valid + invalid
///                        input fallback), interrupt control_response (true/false)
///   server parsers     : control_request subtype/id extraction;
///                        control_response_decision (allow / error / deny)
///   bridge parsers     : SDKControlRequest (initialize, set_model,
///                        set_max_thinking_tokens, set_permission_mode,
///                        interrupt, unknown); SDKControlResponse
///                        (success / error); is_sdk_control_* guards
///   bridge serializers : build_control_response_event (success / error);
///                        handle_server_control_request for every spoken
///                        subtype (initialize, set_model,
///                        set_max_thinking_tokens, set_permission_mode
///                        ok/error/no-callback, interrupt, unknown,
///                        outbound-only rejection)
///
/// The one non-deterministic field the live code emits is the bridge process
/// pid in the initialize response payload; it is normalized to 0 before
/// comparison (see normalize_pid). Everything else is byte-exact.
///
/// Golden management: tests/golden/sdk_control/*.json. Run with
/// UPDATE_GOLDENS=1 to (re)write fixtures, then run normally to verify.

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

import std;

import loom.server.server_main;
import loom.server.server_routes;
import loom.bridge.bridge_messaging;
import loom.hooks.tool_permissions;
import loom.serdes.json;

namespace {

// ---------------------------------------------------------------------------
// Golden fixture helpers (mirrors the sticky_prompt_test::check_golden
// convention in test_ui_helpers.h, but for .json fixtures under
// tests/golden/sdk_control/).
// ---------------------------------------------------------------------------

std::string golden_dir() {
    std::string f = __FILE__;
    auto pos = f.find_last_of('/');
    return f.substr(0, pos + 1) + "golden/sdk_control/";
}

/// The initialize control_response carries the bridge process pid — the one
/// non-deterministic field the live hand-rolled code emits. Freeze the shape,
/// normalize the value. No other spoken control message has a "pid" field.
std::string normalize_pid(std::string json) {
    static const std::regex pid_re(R"("pid":[0-9]+)");
    return std::regex_replace(json, pid_re, R"("pid":0)");
}

/// Result messages carry a non-deterministic uuid (make_id("result") or a
/// bridge UUID v4). Freeze the shape, normalize the value.
std::string normalize_result_uuid(std::string json) {
    static const std::regex uuid_re(R"("uuid":"[^"]*")");
    return std::regex_replace(json, uuid_re, R"("uuid":"result_uuid")");
}

/// Compare a (already-normalized) actual string against the golden fixture.
/// With UPDATE_GOLDENS=1 in the environment, rewrite the fixture instead.
void check_golden_normalized(const std::string& name, const std::string& normalized) {
    const std::string path = golden_dir() + name + ".json";
    if (std::getenv("UPDATE_GOLDENS") != nullptr) {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.good()) << "cannot write golden: " << path;
        out << normalized;
        SUCCEED() << "golden updated: " << path;
        return;
    }
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good()) << "golden missing: " << path
                           << " (run UPDATE_GOLDENS=1 to create)";
    std::string expected((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(normalized, expected)
        << "golden mismatch for '" << name
        << "' (run UPDATE_GOLDENS=1 to refresh)";
}

/// Compare actual output against the golden fixture (byte-exact after pid
/// normalization). With UPDATE_GOLDENS=1 in the environment, rewrite the
/// fixture instead of comparing.
void check_golden(const std::string& name, const std::string& actual) {
    check_golden_normalized(name, normalize_pid(actual));
}

/// Compare result-message output against the golden fixture (byte-exact after
/// pid + uuid normalization — result messages carry a non-deterministic uuid).
void check_golden_result(const std::string& name, const std::string& actual) {
    check_golden_normalized(name, normalize_result_uuid(normalize_pid(actual)));
}

/// Read a golden fixture as a wire-input sample for parser tests.
std::string read_golden(const std::string& name) {
    const std::string path = golden_dir() + name + ".json";
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        ADD_FAILURE() << "golden missing: " << path
                      << " (run UPDATE_GOLDENS=1 to create)";
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// Bring the hand-rolled server functions into scope. They live in
// cc::server::detail (exported by cc.server.server_main) — the detail
// namespace is a naming convention, not an access boundary.
namespace srv = cc::server::detail;

/// Drive handle_server_control_request with a write_event sink and return the
/// exact event string it emitted.
std::string run_handle_request(
    const cc::bridge::SDKControlRequest& request,
    cc::bridge::ServerControlRequestHandlers handlers
) {
    std::string captured;
    handlers.write_event = [&captured](const std::string& event) {
        captured = event;
    };
    cc::bridge::handle_server_control_request(request, handlers);
    return captured;
}

cc::bridge::SDKControlRequest make_control_request(
    std::string request_id,
    std::string subtype
) {
    cc::bridge::SDKControlRequest req;
    req.request_id = std::move(request_id);
    req.request.subtype = std::move(subtype);
    return req;
}

} // namespace

// ===========================================================================
// Server serializers — exact bytes the live server speaks
// ===========================================================================

TEST(SdkControlGolden, ServerCanUseToolRequest) {
    srv::DirectPermissionRequest request{
        .request_id = "perm_req_001",
        .session_id = "sess_001",
        .tool_name = "Bash",
        .input_json = R"({"command":"ls -la","description":"list files"})",
        .tool_use_id = "toolu_001",
    };
    check_golden("server_can_use_tool_request",
                 srv::permission_control_request_json(request));
}

TEST(SdkControlGolden, ServerCanUseToolRequestInvalidInputFallsBackToEmptyObject) {
    srv::DirectPermissionRequest request{
        .request_id = "perm_req_002",
        .session_id = "sess_001",
        .tool_name = "Bash",
        .input_json = "not valid json",
        .tool_use_id = "toolu_002",
    };
    check_golden("server_can_use_tool_request_invalid_input",
                 srv::permission_control_request_json(request));
}

TEST(SdkControlGolden, ServerInterruptResponseTrue) {
    check_golden("server_interrupt_response_true",
                 srv::interrupt_control_response_json("ctrl_req_001", true));
}

TEST(SdkControlGolden, ServerInterruptResponseFalse) {
    check_golden("server_interrupt_response_false",
                 srv::interrupt_control_response_json("ctrl_req_002", false));
}

// ===========================================================================
// Server parsers — control_request subtype/id extraction
// ===========================================================================

TEST(SdkControlGolden, ServerExtractsControlRequestSubtypeAndId) {
    struct Case {
        std::string fixture;
        std::string subtype;
        std::string id;
    };
    const Case cases[] = {
        {"bridge_control_request_initialize", "initialize", "init_req_001"},
        {"bridge_control_request_set_model", "set_model", "model_req_001"},
        {"bridge_control_request_set_max_thinking_tokens",
         "set_max_thinking_tokens", "think_req_001"},
        {"bridge_control_request_set_permission_mode",
         "set_permission_mode", "mode_req_001"},
        {"bridge_control_request_interrupt", "interrupt", "intr_req_001"},
        {"bridge_control_request_unknown", "reload_config", "unk_req_001"},
    };
    for (const auto& c : cases) {
        const auto wire = read_golden(c.fixture);
        auto subtype = srv::control_request_subtype(wire);
        ASSERT_TRUE(subtype.has_value()) << c.fixture;
        EXPECT_EQ(*subtype, c.subtype) << c.fixture;
        EXPECT_EQ(srv::control_request_id(wire), c.id) << c.fixture;
    }
}

TEST(SdkControlGolden, ServerControlRequestSubtypeRejectsNonControl) {
    EXPECT_FALSE(srv::control_request_subtype(R"({"type":"user","message":{}})").has_value());
    EXPECT_FALSE(srv::control_request_subtype("not json").has_value());
    // Missing request_id → generated fallback id (non-deterministic, so only
    // shape is asserted, not golden-ed).
    const auto fallback = srv::control_request_id(
        R"({"type":"control_request","request":{"subtype":"interrupt"}})");
    EXPECT_FALSE(fallback.empty());
    EXPECT_NE(fallback.find("control_"), std::string::npos);
}

// ===========================================================================
// Server parser — control_response_decision (permission answers)
// ===========================================================================

TEST(SdkControlGolden, ServerParsesControlResponseAllow) {
    const auto decision = srv::control_response_decision(
        read_golden("server_control_response_allow"));
    ASSERT_TRUE(decision.has_value());
    EXPECT_EQ(decision->request_id, "perm_req_100");
    EXPECT_EQ(decision->response.decision, cc::hooks::PermissionDecision::allow);
    ASSERT_TRUE(decision->response.updated_input_json.has_value());
    EXPECT_EQ(*decision->response.updated_input_json, R"({"command":"ls"})");
    ASSERT_TRUE(decision->response.updated_permissions_json.has_value());
    EXPECT_EQ(*decision->response.updated_permissions_json,
              R"([{"tool_name":"Bash"}])");
    ASSERT_TRUE(decision->response.message.has_value());
    EXPECT_EQ(*decision->response.message, "ok");
}

TEST(SdkControlGolden, ServerParsesControlResponseError) {
    const auto decision = srv::control_response_decision(
        read_golden("server_control_response_error"));
    ASSERT_TRUE(decision.has_value());
    EXPECT_EQ(decision->request_id, "perm_req_101");
    // Error subtype → deny with the error string as the message.
    EXPECT_EQ(decision->response.decision, cc::hooks::PermissionDecision::deny);
    ASSERT_TRUE(decision->response.message.has_value());
    EXPECT_EQ(*decision->response.message, "user denied");
    EXPECT_FALSE(decision->response.updated_input_json.has_value());
    EXPECT_FALSE(decision->response.updated_permissions_json.has_value());
}

TEST(SdkControlGolden, ServerParsesControlResponseDeny) {
    const auto decision = srv::control_response_decision(
        read_golden("server_control_response_deny"));
    ASSERT_TRUE(decision.has_value());
    EXPECT_EQ(decision->request_id, "perm_req_102");
    EXPECT_EQ(decision->response.decision, cc::hooks::PermissionDecision::deny);
    EXPECT_FALSE(decision->response.message.has_value());
    EXPECT_FALSE(decision->response.updated_input_json.has_value());
    EXPECT_FALSE(decision->response.updated_permissions_json.has_value());
}

TEST(SdkControlGolden, ServerControlResponseDecisionRejectsNonControl) {
    EXPECT_FALSE(srv::control_response_decision(
        R"({"type":"control_request","request":{}})").has_value());
    EXPECT_FALSE(srv::control_response_decision("not json").has_value());
}

// ===========================================================================
// Bridge parsers — SDKControlRequest for every spoken subtype
// ===========================================================================

TEST(SdkControlGolden, BridgeParsesControlRequestInitialize) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_request_initialize"));
    ASSERT_TRUE(parsed.has_value());
    auto root = parsed->root();
    EXPECT_TRUE(cc::bridge::is_sdk_control_request(root));
    const auto req = cc::bridge::parse_control_request(root);
    EXPECT_EQ(req.request_id, "init_req_001");
    EXPECT_EQ(req.request.subtype, "initialize");
    EXPECT_FALSE(req.request.model.has_value());
    EXPECT_FALSE(req.request.mode.has_value());
    EXPECT_FALSE(req.request.max_thinking_tokens.has_value());
}

TEST(SdkControlGolden, BridgeParsesControlRequestSetModel) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_request_set_model"));
    ASSERT_TRUE(parsed.has_value());
    const auto req = cc::bridge::parse_control_request(parsed->root());
    EXPECT_EQ(req.request_id, "model_req_001");
    EXPECT_EQ(req.request.subtype, "set_model");
    ASSERT_TRUE(req.request.model.has_value());
    EXPECT_EQ(*req.request.model, "claude-sonnet-4");
}

TEST(SdkControlGolden, BridgeParsesControlRequestSetMaxThinkingTokens) {
    auto parsed = cc::utils::json::parse(
        read_golden("bridge_control_request_set_max_thinking_tokens"));
    ASSERT_TRUE(parsed.has_value());
    const auto req = cc::bridge::parse_control_request(parsed->root());
    EXPECT_EQ(req.request_id, "think_req_001");
    EXPECT_EQ(req.request.subtype, "set_max_thinking_tokens");
    ASSERT_TRUE(req.request.max_thinking_tokens.has_value());
    EXPECT_EQ(*req.request.max_thinking_tokens, 1024);
}

TEST(SdkControlGolden, BridgeParsesControlRequestSetPermissionMode) {
    auto parsed = cc::utils::json::parse(
        read_golden("bridge_control_request_set_permission_mode"));
    ASSERT_TRUE(parsed.has_value());
    const auto req = cc::bridge::parse_control_request(parsed->root());
    EXPECT_EQ(req.request_id, "mode_req_001");
    EXPECT_EQ(req.request.subtype, "set_permission_mode");
    ASSERT_TRUE(req.request.mode.has_value());
    EXPECT_EQ(*req.request.mode, "acceptEdits");
}

TEST(SdkControlGolden, BridgeParsesControlRequestInterrupt) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_request_interrupt"));
    ASSERT_TRUE(parsed.has_value());
    const auto req = cc::bridge::parse_control_request(parsed->root());
    EXPECT_EQ(req.request_id, "intr_req_001");
    EXPECT_EQ(req.request.subtype, "interrupt");
}

TEST(SdkControlGolden, BridgeParsesControlRequestUnknown) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_request_unknown"));
    ASSERT_TRUE(parsed.has_value());
    const auto req = cc::bridge::parse_control_request(parsed->root());
    EXPECT_EQ(req.request_id, "unk_req_001");
    EXPECT_EQ(req.request.subtype, "reload_config");
}

// ===========================================================================
// Bridge parsers — SDKControlResponse
// ===========================================================================

TEST(SdkControlGolden, BridgeParsesControlResponseSuccess) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_response_success"));
    ASSERT_TRUE(parsed.has_value());
    auto root = parsed->root();
    EXPECT_TRUE(cc::bridge::is_sdk_control_response(root));
    const auto resp = cc::bridge::parse_control_response(root);
    EXPECT_EQ(resp.response.subtype, "success");
    EXPECT_EQ(resp.response.request_id, "perm_req_200");
    EXPECT_FALSE(resp.response.error.has_value());
    ASSERT_TRUE(resp.response.response_json.has_value());
    EXPECT_EQ(*resp.response.response_json, R"({"behavior":"allow"})");
}

TEST(SdkControlGolden, BridgeParsesControlResponseError) {
    auto parsed = cc::utils::json::parse(read_golden("bridge_control_response_error"));
    ASSERT_TRUE(parsed.has_value());
    const auto resp = cc::bridge::parse_control_response(parsed->root());
    EXPECT_EQ(resp.response.subtype, "error");
    EXPECT_EQ(resp.response.request_id, "perm_req_201");
    ASSERT_TRUE(resp.response.error.has_value());
    EXPECT_EQ(*resp.response.error, "nope");
    EXPECT_FALSE(resp.response.response_json.has_value());
}

TEST(SdkControlGolden, BridgeTypeGuardsRejectForeignShapes) {
    auto user_msg = cc::utils::json::parse(
        R"({"type":"user","message":{"content":"hi"}})");
    ASSERT_TRUE(user_msg.has_value());
    EXPECT_FALSE(cc::bridge::is_sdk_control_request(user_msg->root()));
    EXPECT_FALSE(cc::bridge::is_sdk_control_response(user_msg->root()));
}

// ===========================================================================
// Bridge serializers — build_control_response_event
// ===========================================================================

TEST(SdkControlGolden, BridgeBuildsControlResponseEventSuccess) {
    check_golden("bridge_control_response_event_success",
                 cc::bridge::build_control_response_event(
                     "sess_001", "req_300", "success", "{}"));
}

TEST(SdkControlGolden, BridgeBuildsControlResponseEventError) {
    check_golden("bridge_control_response_event_error",
                 cc::bridge::build_control_response_event(
                     "sess_001", "req_301", "error", "{}", "boom"));
}

// ===========================================================================
// Bridge serializers — handle_server_control_request for every spoken
// request subtype (the exact event written back to the server)
// ===========================================================================

TEST(SdkControlGolden, BridgeHandlesInitialize) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    check_golden("bridge_handle_initialize",
                 run_handle_request(
                     make_control_request("init_req_001", "initialize"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesSetModel) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.on_set_model = [](const std::optional<std::string>&) {};
    check_golden("bridge_handle_set_model",
                 run_handle_request(
                     make_control_request("model_req_001", "set_model"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesSetMaxThinkingTokens) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.on_set_max_thinking_tokens = [](std::optional<int64_t>) {};
    check_golden("bridge_handle_set_max_thinking_tokens",
                 run_handle_request(
                     make_control_request("think_req_001", "set_max_thinking_tokens"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesSetPermissionModeOk) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.on_set_permission_mode = [](const std::string&) {
        return std::pair<bool, std::string>{true, ""};
    };
    check_golden("bridge_handle_set_permission_mode_ok",
                 run_handle_request(
                     make_control_request("mode_req_001", "set_permission_mode"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesSetPermissionModeError) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.on_set_permission_mode = [](const std::string&) {
        return std::pair<bool, std::string>{false, "mode rejected"};
    };
    check_golden("bridge_handle_set_permission_mode_err",
                 run_handle_request(
                     make_control_request("mode_req_002", "set_permission_mode"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesSetPermissionModeNoCallback) {
    // No on_set_permission_mode registered → error (not a silent false
    // success).
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    check_golden("bridge_handle_set_permission_mode_no_callback",
                 run_handle_request(
                     make_control_request("mode_req_003", "set_permission_mode"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesInterrupt) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.on_interrupt = [] {};
    check_golden("bridge_handle_interrupt",
                 run_handle_request(
                     make_control_request("intr_req_001", "interrupt"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesUnknownSubtype) {
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    check_golden("bridge_handle_unknown",
                 run_handle_request(
                     make_control_request("unk_req_001", "reload_config"),
                     handlers));
}

TEST(SdkControlGolden, BridgeHandlesOutboundOnlyRejectsMutableRequest) {
    // Outbound-only sessions reject every mutable subtype with the fixed
    // OUTBOUND_ONLY_ERROR text (initialize still succeeds — covered above).
    cc::bridge::ServerControlRequestHandlers handlers;
    handlers.session_id = "sess_001";
    handlers.outbound_only = true;
    check_golden("bridge_handle_outbound_only",
                 run_handle_request(
                     make_control_request("model_req_002", "set_model"),
                     handlers));
}

// ===========================================================================
// Result messages — the SDK stdout wire-message family (server_main.cppm
// :572-614, server_routes.cppm:180-198, bridge_messaging.cppm:647-684).
//
// The golden gate (S6) previously covered only the 21-subtype control
// protocol; the result serializers sat outside its scope, so the
// modelUsage/model_usage wire drift went undetected. These fixtures freeze
// the exact result-message bytes the live code speaks — in particular the
// "modelUsage" key (camelCase, matching the TS schema) and the nested
// "server_tool_use" object inside "usage" — so a future rewire of the live
// emitters to cc.server.control_protocol is verified byte-for-byte.
// ===========================================================================

TEST(SdkControlGolden, ServerResultMessage) {
    auto resp = cc::utils::json::parse(
        R"({"id":"msg_001","response":"done","model":"claude-sonnet-4",)"
        R"("elapsed_ms":1500,"usage":{"input_tokens":100,"output_tokens":50}})");
    ASSERT_TRUE(resp.has_value());
    check_golden_result("server_result_message",
                        srv::sdk_result_message("sess_001", resp->root()));
}

TEST(SdkControlGolden, ServerErrorResult) {
    check_golden_result("server_error_result",
                        srv::sdk_error_result("sess_001", "boom"));
}

TEST(SdkControlGolden, ServerResultIngressEvent) {
    // Deterministic (uuid is "result_" + assistant_message_id) — no uuid
    // normalization needed.
    srv::DirectQueryResult qr;
    qr.content = "done";
    qr.model = "claude-sonnet-4";
    qr.input_tokens = 100;
    qr.output_tokens = 50;
    qr.tool_rounds = 2;
    qr.elapsed_ms = 1500;
    check_golden("server_result_ingress_event",
                 srv::sdk_result_ingress_event("sess_001", "msg_001", qr));
}

TEST(SdkControlGolden, BridgeSerializeResultMessage) {
    check_golden_result("bridge_serialize_result_message",
                        cc::bridge::serialize_result_message(
                            cc::bridge::make_result_message("sess_001")));
}
