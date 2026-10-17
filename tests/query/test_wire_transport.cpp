/// @file test_wire_transport.cpp
/// @brief Real HTTP regressions for QueryEngine wire-backend transport.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <httplib.h>

import std;
import loom.query.query_engine;
import loom.query.wire_protocol;
import loom.query.wire_openai;
import loom.tools.tool;
import loom.tools.runtime_registry;
import loom.orchestration.agent;
import loom.serdes.json;

namespace {

namespace fs = std::filesystem;
namespace wire = loom::query::wire;
using namespace loom::core;

struct TransportReply {
    std::string body;
    int status = 200;
    std::string content_type = "text/event-stream";
};

struct TransportRequest {
    std::string path;
    std::string body;
    std::string authorization;
    std::string anthropic_version;
    std::string api_key;
    std::string custom_header;
};

class WireHttpServer {
public:
    explicit WireHttpServer(std::vector<TransportReply> replies) : replies_(std::move(replies)) {
        server_.new_task_queue = [] { return new httplib::ThreadPool(2); };
        server_.Post(R"(/.*)", [this](const httplib::Request& request, httplib::Response& response) {
            std::size_t index;
            {
                std::lock_guard lock(mutex_);
                index = requests_.size();
                requests_.push_back(TransportRequest{
                    request.path, request.body,
                    request.get_header_value("Authorization"),
                    request.get_header_value("anthropic-version"),
                    request.get_header_value("x-api-key"),
                    request.get_header_value("X-Transport-Probe")});
            }
            const auto& reply = replies_[std::min(index, replies_.size() - 1)];
            response.status = reply.status;
            response.set_content(reply.body, reply.content_type);
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ > 0) {
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
    }

    ~WireHttpServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    bool valid() const { return port_ > 0; }
    std::string base_url() const { return std::format("http://127.0.0.1:{}", port_); }
    std::vector<TransportRequest> requests() const {
        std::lock_guard lock(mutex_);
        return requests_;
    }

private:
    std::vector<TransportReply> replies_;
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::vector<TransportRequest> requests_;
};

std::string openai_frame(std::string_view json) {
    return "data: " + std::string(json) + "\n\n";
}

std::string openai_answer_stream() {
    return openai_frame(R"({"choices":[{"delta":{"content":"wire complete"},"finish_reason":null}]})") +
        openai_frame(R"({"choices":[{"delta":{},"finish_reason":"stop"}]})") +
        openai_frame(R"({"choices":[],"usage":{"prompt_tokens":17,"completion_tokens":9}})") +
        openai_frame("[DONE]");
}

std::string interleaved_tool_stream() {
    return openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call-first","type":"function","function":{"name":"Echo","arguments":"{\"text\":\""}},{"index":1,"id":"call-second","type":"function","function":{"name":"Echo","arguments":"{\"text\":\""}}]},"finish_reason":null}]})") +
        openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":1,"function":{"arguments":"second"}},{"index":0,"function":{"arguments":"first"}}]},"finish_reason":null}]})") +
        openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"}"}},{"index":1,"function":{"arguments":"\"}"}}]},"finish_reason":null}]})") +
        openai_frame(R"({"choices":[{"delta":{},"finish_reason":"tool_calls"}]})") +
        openai_frame(R"({"choices":[],"usage":{"prompt_tokens":11,"completion_tokens":5}})") +
        openai_frame("[DONE]");
}

class TransportEcho final : public ITool {
public:
    const ToolDefinition& definition() const override { return definition_; }
    Result<ToolResult> execute(const ToolInput& input) override {
        const auto parsed = loom::utils::json::parse(input.json());
        if (!parsed) return ToolResult::error("Echo received invalid JSON");
        const auto text = parsed->root().get("text");
        if (!text.is_str()) return ToolResult::error("Echo requires text");
        std::string value(text.as_str());
        {
            std::lock_guard lock(mutex_);
            values_.push_back(value);
            parent_session_ids_.push_back(input.parent_session_id.value_or(""));
        }
        return ToolResult::success("echo:" + value);
    }
    bool check_permission(const ToolInput&) const override { return true; }
    std::vector<std::string> values() const {
        std::lock_guard lock(mutex_);
        return values_;
    }
    std::vector<std::string> parent_session_ids() const {
        std::lock_guard lock(mutex_);
        return parent_session_ids_;
    }

private:
    ToolDefinition definition_{
        .name = "Echo", .description = "Echo the provided text",
        .input_schema = InputSchema{.properties = {
            SchemaProperty{.name = "text", .type = "string", .description = "Text to echo", .required = true}}},
        .permission = ToolPermission::ReadOnly};
    mutable std::mutex mutex_;
    std::vector<std::string> values_;
    std::vector<std::string> parent_session_ids_;
};

class WireTransport : public testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (std::uint64_t attempt = 0;; ++attempt) {
            root_ = fs::temp_directory_path() / std::format("loom-wire-transport-{}-{}", stamp, attempt);
            if (fs::create_directory(root_)) break;
        }
    }
    void TearDown() override { fs::remove_all(root_); }

    QueryEngineConfig config(const WireHttpServer& server, const ToolRegistry& registry) const {
        QueryEngineConfig result;
        result.api_key = "transport-api-key";
        result.auth_token = "transport-bearer";
        result.base_url = server.base_url();
        result.wire_api = "openai";
        result.cwd = root_.string();
        result.retry_policy.max_retries = 0;
        result.model_params.model = "transport-model";
        result.thinking_config.mode = ThinkingConfig::Mode::Disabled;
        result.context_window.auto_compact = false;
        result.tools = registry.get_visible_definitions();
        return result;
    }

    static void expect_openai_request(const TransportRequest& request, std::string_view path = "/v1/chat/completions") {
        EXPECT_EQ(request.path, path);
        EXPECT_EQ(request.authorization, "Bearer transport-bearer");
        EXPECT_TRUE(request.anthropic_version.empty());
        EXPECT_TRUE(request.api_key.empty());
    }

    fs::path root_;
};

TEST_F(WireTransport, OpenAiStreamingExecutesInterleavedToolCallsAndSendsToolRoleResults) {
    WireHttpServer server({{interleaved_tool_stream()}, {openai_answer_stream()}});
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    auto echo = std::make_unique<TransportEcho>();
    const auto* observed = echo.get();
    registry.register_tool(std::move(echo));
    QueryEngine engine(config(server, registry), registry);
    std::vector<StreamEvent> events;
    QueryOptions options;
    options.on_event = [&](const auto& event) { events.push_back(event); };
    engine.stream_query("Echo first and second, then finish", options);

    auto values = observed->values();
    std::ranges::sort(values);
    EXPECT_EQ(values, (std::vector<std::string>{"first", "second"}));
    EXPECT_EQ(observed->parent_session_ids(),
        (std::vector<std::string>{engine.session_id().str(), engine.session_id().str()}));
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event) {
        return std::holds_alternative<StreamError>(event);
    }), 0);
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event) {
        return std::holds_alternative<StreamEnd>(event);
    }), 2);
    const auto requests = server.requests();
    ASSERT_EQ(requests.size(), 2u);
    for (const auto& request : requests) expect_openai_request(request);
    const auto parsed = loom::utils::json::parse(requests[1].body);
    ASSERT_TRUE(parsed.has_value());
    std::map<std::string, std::string> tool_results;
    parsed->root().get("messages").iter([&](const auto message) {
        if (message.get("role").as_str() == "tool") {
            tool_results.emplace(message.get("tool_call_id").as_str(), message.get("content").as_str());
        }
    });
    EXPECT_EQ(tool_results, (std::map<std::string, std::string>{
        {"call-first", "echo:first"}, {"call-second", "echo:second"}}));
    const auto conversation = engine.get_conversation();
    ASSERT_FALSE(conversation.empty());
    const auto* assistant = std::get_if<AssistantMessage>(&conversation.back());
    ASSERT_NE(assistant, nullptr);
    ASSERT_EQ(assistant->content.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<TextBlock>(assistant->content.front()));
    EXPECT_EQ(std::get<TextBlock>(assistant->content.front()).text, "wire complete");
    EXPECT_EQ(engine.get_usage().input_tokens, 28u);
    EXPECT_EQ(engine.get_usage().output_tokens, 14u);
}

TEST_F(WireTransport, OpenAiNonStreamingUsesPreparedPrefixedEndpointAndParsesResponse) {
    WireHttpServer server({TransportReply{
        R"({"id":"chat-nonstream","model":"transport-model","choices":[{"message":{"role":"assistant","content":"nonstream complete"},"finish_reason":"stop"}],"usage":{"prompt_tokens":13,"completion_tokens":7}})",
        200, "application/json"}});
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    auto settings = config(server, registry);
    settings.base_url = server.base_url() + "/tenant/v1";
    QueryEngine engine(std::move(settings), registry);
    auto result = engine.query("Answer once");
    ASSERT_TRUE(result.has_value()) << result.error().format();
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->message.id.value, "chat-nonstream");
    ASSERT_EQ(result->message.content.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<TextBlock>(result->message.content.front()));
    EXPECT_EQ(std::get<TextBlock>(result->message.content.front()).text, "nonstream complete");
    EXPECT_EQ(result->total_usage.input_tokens, 13u);
    EXPECT_EQ(result->total_usage.output_tokens, 7u);
    const auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    expect_openai_request(requests.front(), "/tenant/v1/chat/completions");
    const auto parsed = loom::utils::json::parse(requests.front().body);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->root().get("stream").as_bool());
}

TEST_F(WireTransport, OpenAiStreamErrorsAndTruncationNeverCommitSuccessfulAssistant) {
    const std::vector<TransportReply> cases{
        {openai_frame(R"({"choices":[{"delta":{"content":"truncated after finish"},"finish_reason":"stop"}]})")},
        {openai_frame("[DONE]")},
        {R"({"error":{"message":"expired credentials"}})", 401, "application/json"},
        {openai_frame(R"({"error":{"message":"in-band failure"}})")},
        {openai_frame(R"({"choices":[{"delta":{"content":"unfinished text"},"finish_reason":null}]})")},
        {openai_answer_stream(), 401},
        {openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"name":"Echo","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})") + openai_frame("[DONE]")},
        {openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"bad","function":{"arguments":"{}"}}]},"finish_reason":"tool_calls"}]})") + openai_frame("[DONE]")},
        {openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"bad","function":{"name":"Echo","arguments":"{broken"}}]},"finish_reason":"tool_calls"}]})") + openai_frame("[DONE]")},
        {openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"bad","function":{"name":"Echo","arguments":"[]"}}]},"finish_reason":"tool_calls"}]})") + openai_frame("[DONE]")},
    };
    for (std::size_t index = 0; index < cases.size(); ++index) {
        SCOPED_TRACE(index);
        WireHttpServer server({cases[index]});
        ASSERT_TRUE(server.valid());
        ToolRegistry registry;
        QueryEngine engine(config(server, registry), registry);
        std::vector<StreamEvent> events;
        QueryOptions options;
        options.on_event = [&](const auto& event) { events.push_back(event); };
        engine.stream_query("Fail safely", options);
        EXPECT_GT(std::ranges::count_if(events, [](const auto& event) {
            return std::holds_alternative<StreamError>(event);
        }), 0);
        EXPECT_EQ(std::ranges::count_if(events, [](const auto& event) {
            return std::holds_alternative<StreamEnd>(event);
        }), 0);
        EXPECT_EQ(std::ranges::count_if(engine.get_conversation(), [](const auto& message) {
            return std::holds_alternative<AssistantMessage>(message);
        }), 0);
    }
}

class CustomTransportBackend final : public wire::WireBackend {
public:
    explicit CustomTransportBackend(std::string base_url)
        : base_url_(std::move(base_url)), delegate_(base_url_) {}
    wire::WireApi api() const noexcept override { return wire::WireApi::OpenAi; }
    std::expected<wire::PreparedRequest, std::string> prepare(const wire::RequestInput& input) const override {
        auto prepared = delegate_.prepare(input);
        if (prepared) {
            prepared->url = base_url_ + "/gateway/prepared-request";
            prepared->headers.emplace_back("X-Transport-Probe", "prepared-header");
        }
        return prepared;
    }
    std::expected<wire::ParsedResponse, std::string> parse_response(std::string_view body) const override {
        return delegate_.parse_response(body);
    }
    std::vector<wire::StreamDelta> parse_stream_event(std::string_view event, std::string_view data) const override {
        return delegate_.parse_stream_event(event, data);
    }
    bool stop_reason_is_tool_use(std::string_view reason) const override {
        return delegate_.stop_reason_is_tool_use(reason);
    }

private:
    std::string base_url_;
    wire::OpenAiWireBackend delegate_;
};

TEST_F(WireTransport, InjectedBackendControlsActualTransportPathAndHeaders) {
    WireHttpServer server({{openai_answer_stream()}});
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    QueryEngine engine(config(server, registry), registry);
    engine.set_wire_backend_factory([url = server.base_url()] {
        return std::make_unique<CustomTransportBackend>(url);
    });
    std::vector<StreamEvent> events;
    QueryOptions options;
    options.on_event = [&](const auto& event) { events.push_back(event); };
    engine.stream_query("Use the prepared request", options);
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event) {
        return std::holds_alternative<StreamError>(event);
    }), 0);
    const auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests.front().path, "/gateway/prepared-request");
    EXPECT_EQ(requests.front().custom_header, "prepared-header");
}

} // namespace

TEST_F(WireTransport, DecodesFragmentedCrLfFramesWithoutOptionalSpaces) {
    loom::core::SseEventDecoder decoder;
    EXPECT_TRUE(decoder.feed("event:content_block_delta\r").empty());
    EXPECT_TRUE(decoder.feed("\ndata:{\"text\":\"hello\"}\r\n\r").empty());
    const auto events = decoder.feed("\ndata:[DONE]\r\n\r\n");
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].type, "content_block_delta");
    EXPECT_EQ(events[0].data, R"({"text":"hello"})");
    EXPECT_EQ(events[1].data, "[DONE]");
}

TEST_F(WireTransport, DelegationPolicyAndMetadataReachBothProductionWireFormats) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        for (const auto scenario : {"default", "missing", "denied", "custom", "append", "request-filter"}) {
            SCOPED_TRACE(scenario);
            const std::string messages_answer =
                "event: message_start\ndata: {\"message\":{\"id\":\"prompt-probe\",\"model\":\"transport-model\",\"usage\":{\"input_tokens\":1}}}\n\n"
                "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"wire complete\"}}\n\n"
                "event: content_block_stop\ndata: {\"index\":0}\n\n"
                "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":1}}\n\n"
                "event: message_stop\ndata: {}\n\n";
            WireHttpServer server({{std::string_view(protocol) == "messages"
                ? messages_answer : openai_answer_stream()}});
            ASSERT_TRUE(server.valid());
            ToolRegistry registry;
            loom::tools::RuntimeToolOptions runtime_options;
            runtime_options.agent_tool_factory = loom::tools::make_agent_tool;
            runtime_options.default_model = "transport-model";
            loom::tools::register_runtime_tools(registry, std::move(runtime_options));
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            if (std::string_view(scenario) == "missing") {
                std::erase_if(settings.tools, [](const auto& tool) { return tool.name == "Agent"; });
            } else if (std::string_view(scenario) == "denied") {
                settings.always_deny_rules = {"Agent"};
            } else if (std::string_view(scenario) == "custom") {
                settings.custom_system_prompt = "CUSTOM_WIRE_PROMPT";
            } else if (std::string_view(scenario) == "append") {
                settings.append_system_prompt = "APPEND_WIRE_CONSTRAINT";
            }
            QueryEngine engine(std::move(settings), registry);
            QueryOptions options;
            if (std::string_view(scenario) == "request-filter") options.enabled_tools = {"task_output"};
            std::vector<StreamEvent> events;
            options.on_event = [&](const auto& event) { events.push_back(event); };
            engine.stream_query("Return a short answer without using tools", options);
            EXPECT_EQ(std::ranges::count_if(events, [](const auto& event) {
                return std::holds_alternative<StreamError>(event);
            }), 0);
            const auto requests = server.requests();
            ASSERT_EQ(requests.size(), 1u);
            EXPECT_EQ(requests.front().path, std::string_view(protocol) == "messages"
                ? "/v1/messages" : "/v1/chat/completions");
            auto parsed = loom::utils::json::parse(requests.front().body);
            ASSERT_TRUE(parsed);
            const auto root = parsed->root();
            std::string system;
            if (std::string_view(protocol) == "messages") {
                system = root.get("system").as_str();
            } else {
                root.get("messages").iter([&](const auto message) {
                    if (message.get("role").as_str() == "system") system = message.get("content").as_str();
                });
            }
            const auto mode = std::string_view(scenario);
            const bool policy = mode != "missing" && mode != "denied" && mode != "custom";
            EXPECT_EQ(system.find("# Delegation") != std::string::npos, policy);
            EXPECT_EQ(system.find("non-blocking snapshot") != std::string::npos, policy);
            if (policy) {
                EXPECT_NE(system.find("general-purpose"), std::string::npos);
                EXPECT_NE(system.find("Request-specific tool filtering"), std::string::npos);
            }
            if (mode == "custom") {
                EXPECT_TRUE(system.starts_with("CUSTOM_WIRE_PROMPT"));
                EXPECT_EQ(system.find("You are Loom"), std::string::npos);
            } else if (mode == "append") {
                EXPECT_LT(system.find("# Delegation"), system.find("APPEND_WIRE_CONSTRAINT"));
                EXPECT_LT(system.find("APPEND_WIRE_CONSTRAINT"), system.find("<context>"));
            }
            bool agent_advertised = false;
            bool snapshot_advertised = false;
            root.get("tools").iter([&](auto tool) {
                if (std::string_view(protocol) == "openai") tool = tool.get("function");
                if (tool.get("name").as_str() == "Agent") {
                    agent_advertised = true;
                    const auto description = tool.get("description").as_str();
                    EXPECT_NE(description.find("research"), std::string_view::npos);
                    EXPECT_NE(description.find("general-purpose"), std::string_view::npos);
                    EXPECT_NE(description.find("completion"), std::string_view::npos);
                } else if (tool.get("name").as_str() == "task_output") {
                    snapshot_advertised = true;
                    EXPECT_NE(tool.get("description").as_str().find("Returns immediately"), std::string_view::npos);
                }
            });
            EXPECT_EQ(agent_advertised, mode != "missing" && mode != "denied" && mode != "request-filter");
            EXPECT_TRUE(snapshot_advertised);
        }
    }
}
