/// @file test_api.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cerrno>
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

namespace {

// A real peer keeps the response open until the client closes its socket.
// Gates distinguish cancellation from an HTTP timeout or normal completion.
class StreamingRequestPeer {
public:
    explicit StreamingRequestPeer(bool send_body = false, bool denied = false)
        : accepted_(accepted_signal_.get_future()),
          closed_(closed_signal_.get_future()),
          response_gate_(response_signal_.get_future().share()) {
        listener_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listener_ < 0) return;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            listen(listener_, 1) != 0) return;
        socklen_t length = sizeof(address);
        if (getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) != 0) return;
        port_ = ntohs(address.sin_port);
        worker_ = std::thread([this, send_body, denied] {
            const int peer = accept(listener_, nullptr, nullptr);
            if (peer < 0) return;
            {
                std::scoped_lock lock(peer_mutex_);
                peer_ = peer;
            }
#ifdef SO_NOSIGPIPE
            int no_sigpipe = 1;
            setsockopt(peer, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
            char bytes[8192];
            if (recv(peer, bytes, sizeof(bytes), 0) <= 0) {
                close_peer(peer);
                return;
            }
            accepted_signal_.set_value();
            if (denied) {
                response_gate_.wait();
                write_response(peer, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
            } else if (send_body) {
                write_response(peer, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
                                     "Connection: close\r\n\r\nevent: ping\ndata: {}\n\n");
            }
            while (!stopping_.load()) {
                const auto count = recv(peer, bytes, sizeof(bytes), 0);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0) {
                    // Closing with an unread raced 403 response may reset TCP
                    // rather than send FIN. Both prove the transfer released
                    // its socket; unrelated receive failures remain failures.
                    closed_signal_.set_value(count == 0 || errno == ECONNRESET);
                    break;
                }
            }
            close_peer(peer);
        });
    }

    ~StreamingRequestPeer() {
        release_response();
        stopping_.store(true);
        {
            std::scoped_lock lock(peer_mutex_);
            if (peer_ >= 0) shutdown(peer_, SHUT_RDWR);
        }
        if (listener_ >= 0) shutdown(listener_, SHUT_RDWR);
        if (worker_.joinable()) worker_.join();
        if (listener_ >= 0) close(listener_);
    }

    int port() const { return port_; }
    std::string url() const { return std::format("http://127.0.0.1:{}", port_); }
    bool wait_for_request() { return accepted_.wait_for(std::chrono::seconds(5)) == std::future_status::ready; }
    bool wait_for_close() {
        return closed_.wait_for(std::chrono::seconds(5)) == std::future_status::ready && closed_.get();
    }
    void release_response() {
        if (!released_.exchange(true)) response_signal_.set_value();
    }

private:
    static void write_response(int peer, std::string_view response) {
        while (!response.empty()) {
#ifdef MSG_NOSIGNAL
            const auto count = send(peer, response.data(), response.size(), MSG_NOSIGNAL);
#else
            const auto count = send(peer, response.data(), response.size(), 0);
#endif
            if (count <= 0) return;
            response.remove_prefix(static_cast<size_t>(count));
        }
    }
    void close_peer(int peer) {
        std::scoped_lock lock(peer_mutex_);
        peer_ = -1;
        close(peer);
    }

    int listener_ = -1;
    int port_ = -1;
    int peer_ = -1;
    std::mutex peer_mutex_;
    std::thread worker_;
    std::promise<void> accepted_signal_;
    std::future<void> accepted_;
    std::promise<bool> closed_signal_;
    std::future<bool> closed_;
    std::promise<void> response_signal_;
    std::shared_future<void> response_gate_;
    std::atomic<bool> released_{false};
    std::atomic<bool> stopping_{false};
};

loom::services::api::CreateMessageRequest lifecycle_stream_request() {
    loom::services::api::CreateMessageRequest request;
    request.model = "test-model";
    request.max_tokens = 8;
    request.messages.push_back(loom::services::api::Message::from_text("user", "test"));
    return request;
}

} // namespace

TEST(ApiStreamingLifecycle, DroppingLastConsumerCancelsBeforeResponseHeaders) {
    EnvironmentGuard bedrock("LOOM_USE_BEDROCK", "0");
    EnvironmentGuard vertex("LOOM_USE_VERTEX", "0");
    EnvironmentGuard foundry("LOOM_USE_FOUNDRY", "0");
    StreamingRequestPeer peer;
    ASSERT_GT(peer.port(), 0);
    loom::services::api::MessagesClient::Config config;
    config.base_url = peer.url();
    config.api_key = "test";
    loom::services::api::MessagesClient client(std::move(config));
    auto stream = client.create_message_stream(lifecycle_stream_request());
    ASSERT_TRUE(stream);
    ASSERT_TRUE(peer.wait_for_request());
    auto consumer_copy = *stream;
    stream = std::unexpected(loom::utils::Error(loom::utils::ErrorCode::cancelled, "consumer released"));
    // The surviving consumer remains usable; only its final release cancels.
    EXPECT_FALSE(consumer_copy.is_finished());
    consumer_copy = loom::services::api::StreamParser{};
    EXPECT_TRUE(peer.wait_for_close());
}

TEST(ApiStreamingLifecycle, DroppingLastConsumerCancelsStalledBody) {
    EnvironmentGuard bedrock("LOOM_USE_BEDROCK", "0");
    EnvironmentGuard vertex("LOOM_USE_VERTEX", "0");
    EnvironmentGuard foundry("LOOM_USE_FOUNDRY", "0");
    StreamingRequestPeer peer(true);
    ASSERT_GT(peer.port(), 0);
    loom::services::api::MessagesClient::Config config;
    config.base_url = peer.url();
    config.api_key = "test";
    loom::services::api::MessagesClient client(std::move(config));
    auto stream = client.create_message_stream(lifecycle_stream_request());
    ASSERT_TRUE(stream);
    ASSERT_TRUE(peer.wait_for_request());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool saw_ping = false;
    while (!saw_ping && std::chrono::steady_clock::now() < deadline) {
        auto event = stream->next_event();
        ASSERT_TRUE(event);
        saw_ping = event->has_value();
        if (!saw_ping) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(saw_ping);
    stream = std::unexpected(loom::utils::Error(loom::utils::ErrorCode::cancelled, "consumer released"));
    EXPECT_TRUE(peer.wait_for_close());
}

TEST(ApiStreamingLifecycle, ClientDestructionCancelsLiveConsumer) {
    EnvironmentGuard bedrock("LOOM_USE_BEDROCK", "0");
    EnvironmentGuard vertex("LOOM_USE_VERTEX", "0");
    EnvironmentGuard foundry("LOOM_USE_FOUNDRY", "0");
    StreamingRequestPeer peer;
    ASSERT_GT(peer.port(), 0);
    loom::services::api::MessagesClient::Config config;
    config.base_url = peer.url();
    config.api_key = "test";
    auto client = std::make_unique<loom::services::api::MessagesClient>(std::move(config));
    auto stream = client->create_message_stream(lifecycle_stream_request());
    ASSERT_TRUE(stream);
    ASSERT_TRUE(peer.wait_for_request());
    client.reset();
    EXPECT_TRUE(peer.wait_for_close());
    EXPECT_TRUE(stream->is_finished());
    EXPECT_TRUE(stream->has_error());
}

TEST(ApiStreamingLifecycle, ConcurrentClientAndConsumerDestructionWithPendingEnterpriseError) {
    EnvironmentGuard bedrock("LOOM_USE_BEDROCK", "0");
    EnvironmentGuard vertex("LOOM_USE_VERTEX", "0");
    EnvironmentGuard foundry("LOOM_USE_FOUNDRY", "1");
    EnvironmentGuard skip_auth("LOOM_SKIP_FOUNDRY_AUTH", "1");
    StreamingRequestPeer peer(false, true);
    ASSERT_GT(peer.port(), 0);
    EnvironmentGuard endpoint("LOOM_FOUNDRY_BASE_URL", peer.url());
    loom::services::api::MessagesClient::Config config;
    config.base_url = peer.url();
    config.api_key = "test";
    auto client = std::make_unique<loom::services::api::MessagesClient>(std::move(config));
    auto stream = client->create_message_stream(lifecycle_stream_request());
    ASSERT_TRUE(stream);
    ASSERT_TRUE(peer.wait_for_request());
    std::barrier teardown(3);
    std::thread destroy_consumer([&] {
        teardown.arrive_and_wait();
        stream = std::unexpected(loom::utils::Error(loom::utils::ErrorCode::cancelled, "consumer released"));
    });
    std::thread destroy_client([&] {
        teardown.arrive_and_wait();
        client.reset();
    });
    teardown.arrive_and_wait();
    peer.release_response();
    destroy_consumer.join();
    destroy_client.join();
    EXPECT_TRUE(peer.wait_for_close());
}

TEST(ApiStreamingLifecycle, EnterpriseErrorRemainsReadableAfterClientDestruction) {
    EnvironmentGuard bedrock("LOOM_USE_BEDROCK", "0");
    EnvironmentGuard vertex("LOOM_USE_VERTEX", "0");
    EnvironmentGuard foundry("LOOM_USE_FOUNDRY", "1");
    EnvironmentGuard skip_auth("LOOM_SKIP_FOUNDRY_AUTH", "1");
    StreamingRequestPeer peer(false, true);
    ASSERT_GT(peer.port(), 0);
    EnvironmentGuard endpoint("LOOM_FOUNDRY_BASE_URL", peer.url());
    loom::services::api::MessagesClient::Config config;
    config.base_url = peer.url();
    config.api_key = "test";
    auto client = std::make_unique<loom::services::api::MessagesClient>(std::move(config));
    auto stream = client->create_message_stream(lifecycle_stream_request());
    ASSERT_TRUE(stream);
    ASSERT_TRUE(peer.wait_for_request());
    peer.release_response();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!stream->is_finished() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(stream->is_finished());
    client.reset();
    ASSERT_TRUE(stream->error_details());
    EXPECT_EQ(stream->error_details()->http_status, 403);
    EXPECT_TRUE(peer.wait_for_close());
}

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

TEST(ApiClient, ToolResultSerializerEmitsOnlyTrueErrorFlags) {
    using namespace loom::services::api;
    EXPECT_FALSE(ContentBlock{}.is_error);
    CreateMessageRequest request;
    request.model = "typed-outcome";
    request.messages.push_back(Message{.role = "user", .content = {
        ContentBlock{.type = ContentBlockType::ToolResult, .text = "completed", .tool_use_id = "completed"},
        ContentBlock{.type = ContentBlockType::ToolResult, .text = "failed", .tool_use_id = "failed", .is_error = true},
        ContentBlock{.type = ContentBlockType::ToolResult, .text = "legacy success", .tool_use_id = "legacy", .is_error = false}}});
    const auto serialized = RequestSerializer::serialize(request);
    const auto parsed = loom::utils::json::parse(serialized);
    ASSERT_TRUE(parsed);
    const auto blocks = parsed->root().get("messages").at(0).get("content");
    ASSERT_EQ(blocks.size(), 3u);
    EXPECT_FALSE(blocks.at(0).get("is_error").valid());
    EXPECT_TRUE(blocks.at(1).get("is_error").is_bool());
    EXPECT_TRUE(blocks.at(1).get("is_error").as_bool());
    EXPECT_FALSE(blocks.at(2).get("is_error").valid());
    EXPECT_EQ(blocks.at(0).get_string("content"), "completed");
    EXPECT_EQ(blocks.at(1).get_string("tool_use_id"), "failed");
}
