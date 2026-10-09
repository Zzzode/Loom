/// @file test_api.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

#include <gtest/gtest.h>
#include <httplib.h>
#include <limits>



import std;
import loom.cli.ccr_client;
import loom.cli.sse_transport;
import loom.bridge.core;
import loom.config.config;
import loom.constants.paths;
import loom.services.api.client;
import loom.services.api.errors;
import loom.services.api.session_ingress;
import loom.services.api.streaming;
import loom.services.compact.api_microcompact;
import loom.services.lsp.LSPServerManager;
import loom.services.lsp.client;
import loom.services.mcp.client;
import loom.services.mcp.auth;
import loom.services.mcp.channel_permissions;
import loom.services.mcp.config;
import loom.services.mcp.connection_manager;
import loom.services.mcp.elicitation_handler;
import loom.services.mcp.headers_helper;
import loom.services.mcp.vscode_sdk_mcp;
import loom.services.memory.sessionMemory;
import loom.services.extract_memories;
import loom.services.mcp.types;
import loom.services.mcp.xaa;
import loom.services.mcp.xaa_idp_login;
import loom.services.mcp.oauth_port;
import loom.services.rate_limit;
import loom.services.token_estimation;
import loom.services.prompt_suggestion;
import loom.server.server_routes;
import loom.server.server_main;
import loom.session.storage;
import loom.session.history;
import loom.commands.config;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.tools.mcp;
import loom.query.query_engine;
import loom.memdir.paths;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.tools.tool;
import loom.types.types;
import loom.utils.error;
import loom.services.ide_integration;
import loom.serdes.json;
import loom.teams.team_helpers;
import loom.fs.atomic_replace;
import loom.daemon.worker_registry;
import loom.server.types;

namespace fs = std::filesystem;

namespace {

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

} // namespace

TEST(ApiErrors, ClassifiesHttpStatusCodes) {
    using loom::services::api::errors::ApiErrorCategory;
    using loom::services::api::errors::ErrorClassifier;

    EXPECT_EQ(ErrorClassifier::classify_status(401), ApiErrorCategory::Authentication);
    EXPECT_EQ(ErrorClassifier::classify_status(429), ApiErrorCategory::RateLimited);
    EXPECT_EQ(ErrorClassifier::classify_status(529), ApiErrorCategory::Overloaded);
    EXPECT_EQ(ErrorClassifier::classify_status(500), ApiErrorCategory::ServerError);
    EXPECT_EQ(ErrorClassifier::classify_status(400), ApiErrorCategory::InvalidRequest);
}

TEST(ApiErrors, RetryDecisionUsesRetryableCategories) {
    using loom::services::api::errors::ApiErrorCategory;
    using loom::services::api::errors::ApiErrorDetails;
    using loom::services::api::errors::ErrorClassifier;

    ApiErrorDetails rate_limited{};
    rate_limited.category = ApiErrorCategory::RateLimited;
    rate_limited.http_status = 429;
    ApiErrorDetails bad_request{};
    bad_request.category = ApiErrorCategory::InvalidRequest;
    bad_request.http_status = 400;

    EXPECT_TRUE(ErrorClassifier::is_retryable(rate_limited));
    EXPECT_FALSE(ErrorClassifier::is_retryable(bad_request));
}

TEST(ApiErrors, ClientMapsJsonHttpErrorsToStructuredMessages) {
    auto error = loom::services::api::MessagesClient::error_from_http_response(
        400,
        R"({"type":"error","error":{"type":"invalid_request_error","message":"prompt is too long"}})",
        std::optional<std::string>{"req_123"});

    EXPECT_EQ(error.code(), loom::utils::ErrorCode::invalid_argument);
    EXPECT_NE(error.message().find("HTTP 400 invalid_request_error: prompt is too long"), std::string::npos);
    EXPECT_NE(error.message().find("req_123"), std::string::npos);
}

TEST(ApiErrors, ClientPreservesRetryAfterFromJsonHttpErrors) {
    auto error = loom::services::api::MessagesClient::error_from_http_response(
        429,
        R"({"error":{"type":"rate_limit_error","message":"too many requests","retry_after_seconds":7}})");

    EXPECT_EQ(error.code(), loom::utils::ErrorCode::resource_exhausted);
    EXPECT_NE(error.message().find("rate_limit_error: too many requests"), std::string::npos);
    EXPECT_NE(error.message().find("retry after: 7s"), std::string::npos);
}

TEST(ApiErrors, ClientErrorDetailsDriveRetryClassification) {
    using loom::services::api::errors::ApiErrorCategory;
    using loom::services::api::errors::ErrorClassifier;

    auto invalid_error = loom::services::api::MessagesClient::error_from_http_response(
        400,
        R"({"error":{"type":"invalid_request_error","message":"bad tool schema"}})");
    auto invalid_details = loom::services::api::MessagesClient::error_details_from_error(invalid_error);

    EXPECT_EQ(invalid_details.category, ApiErrorCategory::InvalidRequest);
    EXPECT_EQ(invalid_details.http_status, 400);
    EXPECT_EQ(invalid_details.error_type, "invalid_request_error");
    EXPECT_FALSE(ErrorClassifier::is_retryable(invalid_details));

    auto rate_limit_error = loom::services::api::MessagesClient::error_from_http_response(
        429,
        R"({"error":{"type":"rate_limit_error","message":"too many requests","retry_after_seconds":7}})");
    auto rate_limit_details = loom::services::api::MessagesClient::error_details_from_error(rate_limit_error);

    EXPECT_EQ(rate_limit_details.category, ApiErrorCategory::RateLimited);
    EXPECT_EQ(rate_limit_details.retry_after_seconds, std::optional<int>{7});
    EXPECT_TRUE(ErrorClassifier::is_retryable(rate_limit_details));
}

TEST(ApiMicrocompact, BuildsThinkingAndToolContextManagementStrategies) {
    EnvironmentGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS", "1");
    EnvironmentGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES", "true");
    EnvironmentGuard max_tokens_guard("API_MAX_INPUT_TOKENS", "1000");
    EnvironmentGuard target_tokens_guard("API_TARGET_INPUT_TOKENS", "250");

    auto context = loom::services::compact::get_api_context_management({
        .has_thinking = true,
        .is_redact_thinking_active = false,
        .clear_all_thinking = true,
    });

    ASSERT_TRUE(context.has_value());
    ASSERT_EQ(context->edits.size(), 3u);

    EXPECT_EQ(context->edits[0].type, "clear_thinking_20251015");
    EXPECT_TRUE(context->edits[0].has_thinking_keep);
    ASSERT_TRUE(context->edits[0].keep_thinking_turns.has_value());
    EXPECT_EQ(*context->edits[0].keep_thinking_turns, 1u);

    EXPECT_EQ(context->edits[1].type, "clear_tool_uses_20250919");
    ASSERT_TRUE(context->edits[1].trigger_input_tokens.has_value());
    ASSERT_TRUE(context->edits[1].clear_at_least_input_tokens.has_value());
    EXPECT_EQ(*context->edits[1].trigger_input_tokens, 1000u);
    EXPECT_EQ(*context->edits[1].clear_at_least_input_tokens, 750u);
    EXPECT_TRUE(std::ranges::contains(context->edits[1].clear_tool_inputs, std::string{"Bash"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[1].clear_tool_inputs, std::string{"Read"}));

    EXPECT_EQ(context->edits[2].type, "clear_tool_uses_20250919");
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"Edit"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"Write"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"NotebookEdit"}));
}

TEST(ApiClient, MessageFromTextCreatesSingleTextBlock) {
    auto message = loom::services::api::Message::from_text("user", "hello");

    ASSERT_EQ(message.role, "user");
    ASSERT_EQ(message.content.size(), 1u);
    EXPECT_EQ(message.content.front().type, loom::services::api::ContentBlockType::Text);
    EXPECT_EQ(message.content.front().text, "hello");
}

TEST(ApiClient, ResponseCombinesTextContentAndTokenUsage) {
    loom::services::api::CreateMessageResponse response;
    loom::services::api::ContentBlock first;
    first.type = loom::services::api::ContentBlockType::Text;
    first.text = "hello ";
    response.content.push_back(first);
    loom::services::api::ContentBlock second;
    second.type = loom::services::api::ContentBlockType::Text;
    second.text = "world";
    response.content.push_back(second);
    response.usage.input_tokens = 3;
    response.usage.output_tokens = 5;
    response.usage.cache_creation_tokens = 7;
    response.usage.cache_read_tokens = 11;

    EXPECT_EQ(response.get_text_content(), "hello world");
    EXPECT_EQ(response.usage.total(), 8);
    EXPECT_EQ(response.usage.total_with_cache(), 26);
}

TEST(ApiClient, RequestSerializerPreservesToolUseInputJson) {
    loom::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(loom::services::api::Message{
        .role = "assistant",
        .content = {
            loom::services::api::ContentBlock{
                .type = loom::services::api::ContentBlockType::Text,
                .text = "I will read a file."
            },
            loom::services::api::ContentBlock{
                .type = loom::services::api::ContentBlockType::ToolUse,
                .tool_use_id = "toolu_1",
                .tool_name = "Read",
                .tool_input_json = R"({"file_path":"README.md","limit":20})"
            }
        }
    });

    auto serialized = loom::services::api::RequestSerializer::serialize(request);
    auto parsed = loom::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto content = parsed->root().get("messages").at(0).get("content");
    ASSERT_TRUE(content.is_arr());
    auto tool_use = content.at(1);
    EXPECT_EQ(tool_use.get("type").as_str(), "tool_use");
    EXPECT_EQ(tool_use.get("id").as_str(), "toolu_1");
    EXPECT_EQ(tool_use.get("name").as_str(), "Read");
    auto input = tool_use.get("input");
    ASSERT_TRUE(input.is_obj());
    EXPECT_EQ(input.get("file_path").as_str(), "README.md");
    EXPECT_EQ(input.get("limit").as_int(), 20);
}

TEST(ApiClient, RequestSerializerPreservesImageAndDocumentBlocks) {
    loom::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(loom::services::api::Message{
        .role = "user",
        .content = {
            loom::services::api::ContentBlock{
                .type = loom::services::api::ContentBlockType::Image,
                .media_type = "image/png",
                .image_data = "iVBORw0KGgo="
            },
            loom::services::api::ContentBlock{
                .type = loom::services::api::ContentBlockType::Document,
                .media_type = "application/pdf",
                .image_data = "JVBERi0xLjQ="
            }
        }
    });

    auto serialized = loom::services::api::RequestSerializer::serialize(request);
    auto parsed = loom::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto content = parsed->root().get("messages").at(0).get("content");
    ASSERT_TRUE(content.is_arr());

    auto image = content.at(0);
    EXPECT_EQ(image.get("type").as_str(), "image");
    EXPECT_EQ(image.get("source").get("type").as_str(), "base64");
    EXPECT_EQ(image.get("source").get("media_type").as_str(), "image/png");
    EXPECT_EQ(image.get("source").get("data").as_str(), "iVBORw0KGgo=");

    auto document = content.at(1);
    EXPECT_EQ(document.get("type").as_str(), "document");
    EXPECT_EQ(document.get("source").get("type").as_str(), "base64");
    EXPECT_EQ(document.get("source").get("media_type").as_str(), "application/pdf");
    EXPECT_EQ(document.get("source").get("data").as_str(), "JVBERi0xLjQ=");
}

TEST(ApiClient, RequestSerializerSerializesEffortConfig) {
    loom::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(loom::services::api::Message::from_text("user", "hello"));
    request.output_effort = "high";
    request.task_budget = loom::services::api::TaskBudget{
        .total = 12000,
        .remaining = 3456,
    };

    auto serialized = loom::services::api::RequestSerializer::serialize(request);
    auto parsed = loom::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto output_config = parsed->root().get("output_config");
    ASSERT_TRUE(output_config.is_obj());
    EXPECT_EQ(output_config.get("effort").as_str(), "high");
    auto task_budget = output_config.get("task_budget");
    ASSERT_TRUE(task_budget.is_obj());
    EXPECT_EQ(task_budget.get("type").as_str(), "tokens");
    EXPECT_EQ(task_budget.get("total").as_int(), 12000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 3456);
}

TEST(ApiClient, ResponseParserPreservesToolUseInputJson) {
    const auto response = loom::services::api::ResponseParser::parse(R"({
      "id": "msg_1",
      "model": "loom-test",
      "role": "assistant",
      "content": [
        {
          "type": "tool_use",
          "id": "toolu_1",
          "name": "Bash",
          "input": {"command": "pwd", "timeout": 1000}
        }
      ],
      "stop_reason": "tool_use",
      "usage": {"input_tokens": 1, "output_tokens": 2}
    })");

    ASSERT_TRUE(response.has_value()) << response.error().message();
    ASSERT_EQ(response->content.size(), 1u);
    const auto& block = response->content.front();
    EXPECT_EQ(block.type, loom::services::api::ContentBlockType::ToolUse);
    EXPECT_EQ(block.tool_use_id, "toolu_1");
    EXPECT_EQ(block.tool_name, "Bash");

    auto input = loom::utils::json::parse(block.tool_input_json);
    ASSERT_TRUE(input.has_value()) << input.error().message();
    EXPECT_EQ(input->root().get("command").as_str(), "pwd");
    EXPECT_EQ(input->root().get("timeout").as_int(), 1000);
}

TEST(ApiStreaming, SseBufferExtractsCompleteEvents) {
    loom::services::api::SseBuffer buffer;
    buffer.append("event: ping\ndata: {}\n\n");
    auto event = buffer.next_event();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->first, "ping");
    EXPECT_EQ(event->second, "{}");
}

TEST(ApiStreaming, StreamParserAccumulatesTextDeltas) {
    loom::services::api::StreamParser parser;
    parser.start();
    parser.feed("event: content_block_delta\n");
    parser.feed("data: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hi\"}}\n\n");

    auto event = parser.next_event();
    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(event->has_value());
    EXPECT_EQ((*event)->type, loom::services::api::StreamEventType::ContentBlockDelta);
    EXPECT_EQ(parser.full_text(), "hi");
    EXPECT_EQ(parser.statistics().total_events, 1);
}

// End of file
