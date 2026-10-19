/// @file test_wire_transport.cpp
/// @brief Real HTTP regressions for QueryEngine wire-backend transport.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <httplib.h>

import std;
import loom.query.query_engine;
import loom.query.wire_protocol;
import loom.query.assembly;
import loom.config.config;
import loom.session.storage;
import loom.session.history;
import loom.query.wire_openai;
import loom.services.compact.context_compaction;
import loom.services.compact.model_context;
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
    using MetadataHandler = std::function<void(const httplib::Request&, httplib::Response&)>;
    explicit WireHttpServer(std::vector<TransportReply> replies, std::optional<TransportReply> summary = {},
                            MetadataHandler metadata = {})
        : replies_(std::move(replies)), summary_(std::move(summary)) {
        server_.new_task_queue = [] { return new httplib::ThreadPool(2); };
        server_.Get(R"(/.*)", [this, metadata = std::move(metadata)](const httplib::Request& request, httplib::Response& response) {
            {
                std::lock_guard lock(mutex_);
                metadata_requests_.push_back(TransportRequest{
                    request.path, request.body, request.get_header_value("Authorization"),
                    request.get_header_value("anthropic-version"), request.get_header_value("x-api-key"),
                    request.get_header_value("X-Transport-Probe")});
                methods_.push_back("GET");
            }
            if (metadata) metadata(request, response);
            else response.status = 404;
        });
        server_.Post(R"(/.*)", [this](const httplib::Request& request, httplib::Response& response) {
            std::size_t index;
            const bool summary_request = summary_ && request.body.find("CONTEXT CHECKPOINT") != std::string::npos;
            {
                std::lock_guard lock(mutex_);
                index = business_requests_;
                methods_.push_back("POST");
                if (!summary_request) ++business_requests_;
                requests_.push_back(TransportRequest{
                    request.path, request.body,
                    request.get_header_value("Authorization"),
                    request.get_header_value("anthropic-version"),
                    request.get_header_value("x-api-key"),
                    request.get_header_value("X-Transport-Probe")});
            }
            const auto& reply = summary_request ? *summary_ : replies_[std::min(index, replies_.size() - 1)];
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

    std::vector<TransportRequest> metadata_requests() const {
        std::lock_guard lock(mutex_);
        return metadata_requests_;
    }
    std::vector<std::string> methods() const {
        std::lock_guard lock(mutex_);
        return methods_;
    }

private:
    std::vector<TransportReply> replies_;
    std::optional<TransportReply> summary_;
    std::size_t business_requests_ = 0;
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::vector<TransportRequest> requests_;
    std::vector<TransportRequest> metadata_requests_;
    std::vector<std::string> methods_;
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
    explicit TransportEcho(std::function<void(const ToolInput&)> observe = {}) : observe_(std::move(observe)) {}
    const ToolDefinition& definition() const override { return definition_; }
    Result<ToolResult> execute(const ToolInput& input) override {
        const auto parsed = loom::utils::json::parse(input.json());
        if (!parsed) return ToolResult::error("Echo received invalid JSON");
        const auto text = parsed->root().get("text");
        if (!text.is_str()) return ToolResult::error("Echo requires text");
        std::string value(text.as_str());
        if (observe_) observe_(input);
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
    std::function<void(const ToolInput&)> observe_;
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
    explicit CustomTransportBackend(std::string base_url, std::shared_ptr<std::atomic<int>> prepares = {})
        : base_url_(std::move(base_url)), delegate_(base_url_), prepares_(std::move(prepares)) {}
    wire::WireApi api() const noexcept override { return wire::WireApi::OpenAi; }
    std::expected<wire::PreparedRequest, std::string> prepare(const wire::RequestInput& input) const override {
        if (prepares_) ++*prepares_;
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
    std::shared_ptr<std::atomic<int>> prepares_;
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


std::string checkpoint_text() {
    return "## Goal\nComplete verification.\n## Constraints\nNEVER COMMIT; retain LATE_CONSTRAINT_47.\n"
        "## Decisions\nUse local changes.\n## Progress\nCompleted result=47.\n"
        "## Next Steps\nFinish the current request.\n## References\n/work/exact.cpp\n";
}
std::string json_string(std::string_view text) {
    std::string out="\"";
    for (char c:text) {
        if(c=='\n') out+="\\n";
        else if(c=='\"') out+="\\\"";
        else if(c=='\\') out+="\\\\";
        else out+=c;
    }
    return out+"\"";
}
TransportReply compact_reply(std::string_view protocol, bool checkpoint=false) {
    const auto text=json_string(checkpoint ? checkpoint_text() : "wire complete");
    if(protocol=="messages")
        return {"{\"id\":\"compact-test\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":"+text+"}],\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":17,\"output_tokens\":9}}",200,"application/json"};
    return {"{\"id\":\"compact-test\",\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":"+text+"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":17,\"completion_tokens\":9}}",200,"application/json"};
}
std::string compact_messages_stream() {
    return "event: message_start\ndata: {\"message\":{\"id\":\"compact-done\",\"model\":\"transport-model\",\"usage\":{\"input_tokens\":17}}}\n\n"
        "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
        "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"wire complete\"}}\n\n"
        "event: content_block_stop\ndata: {\"index\":0}\n\n"
        "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":9}}\n\n"
        "event: message_stop\ndata: {}\n\n";
}
void isolate_compaction_system(QueryEngine& engine) {
    SystemMessage system{};
    system.id=MessageId{"controlled-compaction-system"};
    system.content.emplace_back(TextBlock{"Verify local work; preserve constraints; never commit, push or publish."});
    engine.restore_conversation({Message{std::move(system)}});
}
void seed_compactable_history(QueryEngine& engine, std::size_t bytes=7000) {
    UserMessage old{};
    old.id=MessageId{"old-constraint"};
    old.content.emplace_back(TextBlock{std::string(bytes,'x')+" LATE_CONSTRAINT_47; NEVER COMMIT"});
    engine.append_message_for_testing(old);
    AssistantMessage progress{};
    progress.id=MessageId{"completed-progress"};
    progress.content.emplace_back(TextBlock{std::string(bytes,'y')+" completed result=47; /work/exact.cpp"});
    engine.append_message_for_testing(progress);
}
std::vector<TransportRequest> business_requests(const WireHttpServer& server) {
    auto all=server.requests();
    std::erase_if(all,[](const auto& request){return request.body.find("CONTEXT CHECKPOINT")!=std::string::npos;});
    return all;
}

std::vector<Message> persisted_audit_messages(const fs::path& sessions, std::string_view id) {
    const auto stored=loom::session::load_messages(sessions,id);
    std::vector<Message> messages;
    for(std::size_t index=0;index<stored.size();++index) {
        auto parsed=loom::query::parse_session_message_value(stored[index].root(),index);
        EXPECT_TRUE(parsed.has_value())<<"Invalid persisted audit message "<<index;
        if(parsed) messages.push_back(std::move(*parsed));
    }
    return messages;
}

TEST_F(WireTransport, ContextOverflowRecoversOnceWithActuallySmallerShortHistory) {
    for(const auto protocol:{"messages","openai"}) {
        for(const auto& error:{TransportReply{R"({"error":{"message":"prompt is too long"}})",400,"application/json"},
                              TransportReply{R"({"error":{"code":"context_length_exceeded","message":"too much input"}})",400,"application/json"},
                              TransportReply{R"({"error":{"message":"request payload too large"}})",413,"application/json"}}) {
            SCOPED_TRACE(protocol);
            SCOPED_TRACE(error.body);
            WireHttpServer server({error,compact_reply(protocol)},compact_reply(protocol,true));
            ToolRegistry registry;
            auto settings=config(server,registry);
            settings.wire_api=protocol;
            settings.custom_system_prompt="Verify local work; do not commit.";
            settings.context_window.max_context_tokens=4096;
            settings.model_params.max_tokens=128;
            QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
            seed_compactable_history(engine);
            const auto result=engine.query("CURRENT_REQUEST_EXACT: continue verification.");
            ASSERT_TRUE(result) << result.error().format();
            const auto requests=business_requests(server);
            ASSERT_EQ(requests.size(),2u);
            EXPECT_LT(requests[1].body.size(),requests[0].body.size());
            EXPECT_NE(requests[1].body.find("LATE_CONSTRAINT_47"),std::string::npos);
            EXPECT_NE(requests[1].body.find("NEVER COMMIT"),std::string::npos);
            EXPECT_NE(requests[1].body.find("result=47"),std::string::npos);
            EXPECT_NE(requests[1].body.find("CURRENT_REQUEST_EXACT"),std::string::npos);
        }
    }
}

TEST_F(WireTransport, ProactiveCompactionBoundsLargeRecentHistoryBeforeRequest) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        WireHttpServer server({compact_reply(protocol)},compact_reply(protocol,true));
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work; do not commit.";
        settings.context_window.auto_compact=true;
        settings.context_window.max_context_tokens=4096;
        settings.context_window.compaction_threshold=0.6;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine,10000);
        ASSERT_EQ(server.requests().size(),0u) << "Append must not perform model requests";
        const auto result=engine.query("CURRENT_REQUEST_EXACT: continue.");
        ASSERT_TRUE(result) << result.error().format();
        const auto requests=business_requests(server);
        ASSERT_EQ(requests.size(),1u);
        EXPECT_LT(requests.front().body.size(),10000u);
        EXPECT_NE(requests.front().body.find("## Constraints"),std::string::npos);
        EXPECT_NE(requests.front().body.find("CURRENT_REQUEST_EXACT"),std::string::npos);
    }
}

TEST_F(WireTransport, InBandOverflowRecoversOnceAndDiscardsPartialDraft) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        const auto overflow=std::string_view(protocol)=="openai"
            ? openai_frame(R"({"choices":[{"delta":{"content":"discard this draft"},"finish_reason":null}]})")+
              openai_frame(R"({"error":{"code":"context_length_exceeded","message":"prompt is too long"}})")
            : std::string("event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n")+
              "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"discard this draft\"}}\n\n"
              "event: error\ndata: {\"type\":\"error\",\"error\":{\"type\":\"invalid_request_error\",\"message\":\"prompt is too long\"}}\n\n";
        WireHttpServer server({{overflow},{std::string_view(protocol)=="openai"?openai_answer_stream():compact_messages_stream()}},compact_reply(protocol,true));
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine);
        std::vector<StreamEvent> events;
        QueryOptions options;
        options.on_event=[&](const auto& event){events.push_back(event);};
        engine.stream_query("CURRENT_REQUEST_EXACT: continue.",options);
        EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<StreamError>(event);}),0);
        const auto requests=business_requests(server);
        ASSERT_EQ(requests.size(),2u);
        EXPECT_LT(requests[1].body.size(),requests[0].body.size());
        const auto conversation=engine.get_conversation();
        const auto* last=std::get_if<AssistantMessage>(&conversation.back());
        ASSERT_NE(last,nullptr);
        EXPECT_EQ(std::get<TextBlock>(last->content.front()).text,"wire complete");
    }
}

TEST_F(WireTransport, SecondOverflowStopsWithoutExecutingToolsOrCompactingAgain) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        TransportReply overflow{R"({"error":{"code":"context_length_exceeded","message":"prompt is too long"}})",400,"application/json"};
        WireHttpServer server({overflow,overflow},compact_reply(protocol,true));
        ToolRegistry registry;
        auto tool=std::make_unique<TransportEcho>();
        const auto* observed=tool.get();
        registry.register_tool(std::move(tool));
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine);
        EXPECT_FALSE(engine.query("CURRENT_REQUEST_EXACT: continue."));
        EXPECT_EQ(business_requests(server).size(),2u);
        EXPECT_TRUE(observed->values().empty());
        EXPECT_LE(server.requests().size(),6u);
    }
}

TEST_F(WireTransport, SummaryRequestStripsBusinessOptionsAndPreservesTransportAuthentication) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        WireHttpServer server({compact_reply(protocol)},compact_reply(protocol,true));
        ToolRegistry registry;
        registry.register_tool(std::make_unique<TransportEcho>());
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        settings.task_budget=QueryEngineConfig::TaskBudget{.total=100000};
        settings.response_schema=QueryEngineConfig::ResponseSchema{.name="business-answer",.schema_json=R"({"type":"object"})"};
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine);
        ASSERT_TRUE(engine.compact_conversation());
        const auto requests=server.requests();
        ASSERT_FALSE(requests.empty());
        for(const auto& request:requests) {
            EXPECT_NE(request.body.find("CONTEXT CHECKPOINT"),std::string::npos);
            EXPECT_EQ(request.authorization,"Bearer transport-bearer");
            const auto parsed=loom::utils::json::parse(request.body);
            ASSERT_TRUE(parsed);
            const auto root=parsed->root();
            EXPECT_FALSE(root.get("tools").is_arr());
            EXPECT_FALSE(root.get("tool_choice").is_obj());
            EXPECT_FALSE(root.get("thinking").is_obj());
            EXPECT_FALSE(root.get("context_management").is_obj());
            EXPECT_FALSE(root.get("output_config").is_obj());
            EXPECT_FALSE(root.get("response_format").is_obj());
            EXPECT_LE(root.get("max_tokens").as_int(),4096);
        }
        EXPECT_GT(engine.get_usage().input_tokens,0u);
        EXPECT_GT(engine.get_usage().output_tokens,0u);
    }
}

TEST_F(WireTransport, UncompressibleLatestRequestFailsWithoutFalseRetry) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        TransportReply overflow{R"({"error":{"message":"prompt is too long"}})",400,"application/json"};
        WireHttpServer server({overflow},compact_reply(protocol,true));
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        EXPECT_FALSE(engine.query(std::string(40000,'q')+" DO NOT LOSE THE LATEST REQUEST"));
        EXPECT_EQ(business_requests(server).size(),1u);
        EXPECT_EQ(server.requests().size(),1u);
    }
}


TEST_F(WireTransport, CheckpointResumeReplaysOnlyPostBoundaryAuditAndIgnoresMalformedRecords) {
    WireHttpServer server({compact_reply("messages")},compact_reply("messages",true));
    ToolRegistry registry;
    auto settings=config(server,registry);
    settings.wire_api="messages";
    settings.custom_system_prompt="Verify local work.";
    settings.context_window.max_context_tokens=4096;
    settings.model_params.max_tokens=128;
    settings.session_id_override="checkpoint-resume-test";
    QueryEngine engine(settings,registry);
        isolate_compaction_system(engine);
    engine.set_session_storage(root_/"sessions");
    seed_compactable_history(engine);
    UserMessage current{};
    current.id=MessageId{"checkpoint-request"};
    current.content.emplace_back(TextBlock{"CURRENT_REQUEST_EXACT: continue."});
    engine.append_message_for_testing(current);
    ASSERT_TRUE(engine.compact_conversation());
    const auto checkpoint_file=root_/"sessions"/"checkpoint-resume-test"/"context-checkpoints.jsonl";
    ASSERT_TRUE(fs::is_regular_file(checkpoint_file));
    UserMessage followup{};
    followup.id=MessageId{"post-checkpoint-request"};
    followup.content.emplace_back(TextBlock{"POST_BOUNDARY_EXACT"});
    engine.append_message_for_testing(followup);
    const auto audit=persisted_audit_messages(root_/"sessions","checkpoint-resume-test");
    ASSERT_EQ(audit.size(),4u);
    std::string valid_record;
    {
        std::ifstream input(checkpoint_file);
        for(std::string line;std::getline(input,line);) {
            const auto parsed=loom::utils::json::parse(line);
            if(parsed && parsed->root().get("kind").as_str()=="context_checkpoint") valid_record=line;
        }
    }
    ASSERT_FALSE(valid_record.empty());
    const auto valid_json=loom::utils::json::parse(valid_record);
    ASSERT_TRUE(valid_json);
    loom::utils::json::JsonMutDoc bad_doc;
    auto bad_root=bad_doc.copy_val(valid_json->root());
    auto bad_messages=bad_doc.array();
    valid_json->root().get("working_messages").iter([&](const auto value) {
        bad_messages.append(bad_doc.copy_val(value));
    });
    ToolResultMessage orphan{};
    orphan.id=MessageId{"invalid-orphan-result"};
    orphan.tool_use_id=ToolUseId{"no-matching-call"};
    orphan.content.emplace_back(TextBlock{"INVALID_ORPHAN_CHECKPOINT_MUST_BE_IGNORED"});
    bad_messages.append(ConversationStore::serialize_message(bad_doc,Message{orphan}));
    (void)bad_root.remove("working_messages");
    bad_root.add("working_messages",bad_messages);
    bad_doc.set_root(bad_root);
    std::ofstream(checkpoint_file,std::ios::app)<<bad_doc.to_string()<<"\n{truncated\n{\"kind\":\"context_checkpoint\",\"version\":99}\n";
    QueryEngine resumed(settings,registry);
    resumed.set_session_storage(root_/"sessions");
    resumed.restore_conversation(audit);
    const auto restored=resumed.get_conversation();
    EXPECT_LT(restored.size(),audit.size()+2);
    const auto body=resumed.build_request_body_for_testing();
    EXPECT_NE(body.find("LATE_CONSTRAINT_47"),std::string::npos);
    EXPECT_NE(body.find("CURRENT_REQUEST_EXACT"),std::string::npos);
    EXPECT_NE(body.find("POST_BOUNDARY_EXACT"),std::string::npos);
    EXPECT_EQ(body.find("INVALID_ORPHAN_CHECKPOINT_MUST_BE_IGNORED"),std::string::npos);
    EXPECT_EQ(body.find(std::string(1000,'x')),std::string::npos);
    EXPECT_EQ(body.find(std::string(1000,'y')),std::string::npos);
    const auto followups=std::ranges::count_if(restored,[](const auto& message) {
        return std::visit([](const auto& value){return value.id.value=="post-checkpoint-request";},message);
    });
    EXPECT_EQ(followups,1);
    // Legacy audit-only sessions remain readable without a checkpoint log.
    settings.session_id_override="legacy-checkpoint-test";
    QueryEngine legacy(settings,registry);
    legacy.set_session_storage(root_/"sessions");
    legacy.restore_conversation(audit);
    EXPECT_EQ(legacy.get_conversation().size(),audit.size());
}

TEST_F(WireTransport, CheckpointWriteFailureRetainsOriginalWorkingHistoryAndBudget) {
    WireHttpServer server({compact_reply("messages")},compact_reply("messages",true));
    ToolRegistry registry;
    auto settings=config(server,registry);
    settings.wire_api="messages";
    settings.custom_system_prompt="Verify local work.";
    settings.context_window.max_context_tokens=4096;
    settings.model_params.max_tokens=128;
    settings.task_budget=QueryEngineConfig::TaskBudget{.total=100000};
    settings.session_id_override="checkpoint-write-failure";
    QueryEngine engine(settings,registry);
        isolate_compaction_system(engine);
    engine.set_session_storage(root_/"sessions");
    seed_compactable_history(engine);
    UserMessage current{};
    current.id=MessageId{"write-failure-request"};
    current.content.emplace_back(TextBlock{"CURRENT_REQUEST_EXACT"});
    engine.append_message_for_testing(current);
    const auto before=engine.build_request_body_for_testing();
    const auto before_size=engine.get_conversation().size();
    // A directory in place of the authoritative file forces atomic replacement failure,
    // including when tests run as an elevated user that can ignore chmod permissions.
    fs::remove(root_/"sessions"/"checkpoint-write-failure"/"context-checkpoints.jsonl");
    fs::create_directory(root_/"sessions"/"checkpoint-write-failure"/"context-checkpoints.jsonl");
    EXPECT_FALSE(engine.compact_conversation());
    EXPECT_EQ(engine.get_conversation().size(),before_size);
    EXPECT_EQ(engine.build_request_body_for_testing(),before);
}


TEST_F(WireTransport, SecondInBandOverflowStopsWithoutExecutingPartialTools) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        const bool openai=std::string_view(protocol)=="openai";
        const auto overflow=openai
            ? openai_frame(R"({"error":{"code":"context_length_exceeded","message":"prompt is too long"}})")
            : "event: error\ndata: {\"type\":\"error\",\"error\":{\"type\":\"invalid_request_error\",\"message\":\"prompt is too long\"}}\n\n";
        const auto partial_tool=openai
            ? openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"never-execute","type":"function","function":{"name":"Echo","arguments":"{\"text\":\"must not run\"}"}}]},"finish_reason":null}]})")
            : "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"never-execute\",\"name\":\"Echo\",\"input\":{}}}\n\n"
              "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"text\\\":\\\"must not run\\\"}\"}}\n\n"
              "event: content_block_stop\ndata: {\"index\":0}\n\n";
        WireHttpServer server({{overflow},{partial_tool+overflow}},compact_reply(protocol,true));
        ToolRegistry registry;
        auto echo=std::make_unique<TransportEcho>();
        const auto* observed=echo.get();
        registry.register_tool(std::move(echo));
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine);
        std::vector<StreamEvent> events;
        QueryOptions options;
        options.on_event=[&](const auto& event){events.push_back(event);};
        engine.stream_query("CURRENT_REQUEST_EXACT: continue.",options);
        EXPECT_EQ(business_requests(server).size(),2u);
        EXPECT_TRUE(observed->values().empty());
        EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<StreamError>(event);}),1);
        EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<ToolExecutionStart>(event);}),0);
    }
}


TransportReply compact_tool_reply(std::string_view protocol, int round, std::string_view value) {
    const auto text=json_string(value);
    const auto id="round-"+std::to_string(round);
    if(protocol=="messages")
        return {"{\"id\":\"tool-round\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"tool_use\",\"id\":\""+id+"\",\"name\":\"Echo\",\"input\":{\"text\":"+text+"}}],\"stop_reason\":\"tool_use\",\"usage\":{\"input_tokens\":17,\"output_tokens\":9}}",200,"application/json"};
    const auto arguments=json_string("{\"text\":"+text+"}");
    return {"{\"id\":\"tool-round\",\"choices\":[{\"message\":{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\""+id+"\",\"type\":\"function\",\"function\":{\"name\":\"Echo\",\"arguments\":"+arguments+"}}]},\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":17,\"completion_tokens\":9}}",200,"application/json"};
}

TEST_F(WireTransport, SingleTaskToolRoundsCompactAndContinueWithoutDuplicatingExecution) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        std::vector<TransportReply> replies;
        std::vector<std::string> expected;
        for(int round=0;round<8;++round) {
            expected.push_back("completed round "+std::to_string(round)+" "+std::string(2400,'r'));
            replies.push_back(compact_tool_reply(protocol,round,expected.back()));
        }
        replies.push_back(compact_reply(protocol));
        WireHttpServer server(std::move(replies),compact_reply(protocol,true));
        ToolRegistry registry;
        auto echo=std::make_unique<TransportEcho>();
        const auto* observed=echo.get();
        registry.register_tool(std::move(echo));
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify the same local task.";
        settings.context_window.auto_compact=true;
        settings.context_window.max_context_tokens=4096;
        settings.context_window.compaction_threshold=0.65;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        const std::string human="SINGLE_HUMAN_REQUEST_EXACT: LATE_CONSTRAINT_47; NEVER COMMIT; perform the tool rounds and finish.";
        const auto result=engine.query(human);
        ASSERT_TRUE(result)<<result.error().format();
        EXPECT_EQ(observed->values(),expected);
        const auto requests=business_requests(server);
        ASSERT_EQ(requests.size(),9u);
        EXPECT_GT(server.requests().size(),requests.size())<<"The single task must require a checkpoint";
        for(const auto& request:requests) {
            const auto first=request.body.find(human);
            EXPECT_NE(first,std::string::npos);
            EXPECT_EQ(request.body.find(human,first+human.size()),std::string::npos);
        }
        const auto conversation=engine.get_conversation();
        EXPECT_EQ(std::ranges::count_if(conversation,[&](const auto& message) {
            const auto* user=std::get_if<UserMessage>(&message);
            return user && std::ranges::any_of(user->content,[&](const auto& block) {
                const auto* text=std::get_if<TextBlock>(&block);
                return text && text->text==human;
            });
        }),1);
    }
}

TEST_F(WireTransport, TaskEventsCannotReplaceTheLatestHumanCompactionAnchor) {
    WireHttpServer server({compact_reply("messages")},compact_reply("messages",true));
    ToolRegistry registry;
    auto settings=config(server,registry);
    settings.wire_api="messages";
    settings.custom_system_prompt="Verify local work.";
    settings.context_window.max_context_tokens=4096;
    settings.model_params.max_tokens=128;
    QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
    UserMessage human{};
    human.id=MessageId{"actual-human-anchor"};
    human.content.emplace_back(TextBlock{"HUMAN_ANCHOR_EXACT: NEVER COMMIT"});
    engine.append_message_for_testing(human);
    AssistantMessage work{};
    work.id=MessageId{"work-before-task-event"};
    work.content.emplace_back(TextBlock{std::string(8000,'w')});
    engine.append_message_for_testing(work);
    UserMessage event{};
    event.id=MessageId{"runtime-event-is-not-human"};
    event.origin=TaskEvent{};
    event.content.emplace_back(TextBlock{"RUNTIME_EVENT "+std::string(20000,'e')});
    engine.append_message_for_testing(event);
    ASSERT_TRUE(engine.compact_conversation());
    const auto conversation=engine.get_conversation();
    EXPECT_EQ(std::ranges::count_if(conversation,[](const auto& message) {
        return std::visit([](const auto& value){return value.id.value=="actual-human-anchor";},message);
    }),1);
    EXPECT_EQ(std::ranges::count_if(conversation,[](const auto& message) {
        return std::visit([](const auto& value){return value.id.value=="runtime-event-is-not-human";},message);
    }),0);
    const auto body=engine.build_request_body_for_testing();
    EXPECT_NE(body.find("HUMAN_ANCHOR_EXACT"),std::string::npos);
    EXPECT_EQ(body.find(std::string(2000,'e')),std::string::npos);
}


TEST_F(WireTransport, TwoCheckpointsPreserveCumulativeTaskBudgetAfterFreshRestore) {
    WireHttpServer server({compact_reply("messages")},compact_reply("messages",true));
    ToolRegistry registry;
    auto settings=config(server,registry);
    settings.wire_api="messages";
    settings.custom_system_prompt="Verify local work.";
    settings.context_window.max_context_tokens=4096;
    settings.model_params.max_tokens=128;
    settings.task_budget=QueryEngineConfig::TaskBudget{.total=100000};
    settings.session_id_override="two-checkpoints-task-budget";
    QueryEngine engine(settings,registry);
        isolate_compaction_system(engine);
    engine.set_session_storage(root_/"sessions");
    seed_compactable_history(engine);
    UserMessage first{};
    first.id=MessageId{"first-checkpoint-human"};
    first.content.emplace_back(TextBlock{"FIRST_CURRENT_REQUEST"});
    engine.append_message_for_testing(first);
    const auto remaining=[](const QueryEngine& current) {
        const auto parsed=loom::utils::json::parse(current.build_request_body_for_testing());
        return parsed->root().get("output_config").get("task_budget").get("remaining").as_int();
    };
    ASSERT_TRUE(engine.compact_conversation());
    const auto first_remaining=remaining(engine);
    EXPECT_LT(first_remaining,100000);
    AssistantMessage progress{};
    progress.id=MessageId{"second-checkpoint-progress"};
    progress.content.emplace_back(TextBlock{std::string(9000,'s')+" verified second phase"});
    engine.append_message_for_testing(progress);
    UserMessage second{};
    second.id=MessageId{"second-checkpoint-human"};
    second.content.emplace_back(TextBlock{"SECOND_CURRENT_REQUEST"});
    engine.append_message_for_testing(second);
    ASSERT_TRUE(engine.compact_conversation());
    const auto second_remaining=remaining(engine);
    ASSERT_LT(second_remaining,first_remaining);
    const auto audit=persisted_audit_messages(root_/"sessions","two-checkpoints-task-budget");
    ASSERT_EQ(audit.size(),5u);
    QueryEngine restored(settings,registry);
    restored.set_session_storage(root_/"sessions");
    restored.restore_conversation(audit);
    EXPECT_EQ(remaining(restored),second_remaining)<<"Fresh restore must not resurrect input budget consumed before the latest checkpoint";
    const auto restored_body=restored.build_request_body_for_testing();
    EXPECT_NE(restored_body.find("SECOND_CURRENT_REQUEST"),std::string::npos);
    EXPECT_EQ(restored_body.find(std::string(1000,'s')),std::string::npos);
}

TEST_F(WireTransport, PayloadRecoveryRejectsTokenSmallerButSerializedLargerCheckpoint) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        auto summary=checkpoint_text();
        summary.insert(summary.find("## Next Steps"),std::string(2000,'\\')+"\n");
        auto reply=compact_reply(protocol,true);
        const auto old_text=json_string(checkpoint_text());
        const auto at=reply.body.find(old_text);
        ASSERT_NE(at,std::string::npos);
        reply.body.replace(at,old_text.size(),json_string(summary));
        const TransportReply overflow{R"({"error":{"message":"payload too large"}})",413,"application/json"};
        WireHttpServer server({overflow},reply);
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.custom_system_prompt="Verify local work.";
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        UserMessage historical{};
        historical.id=MessageId{"dense-old-context"};
        historical.content.emplace_back(TextBlock{std::string(3000,'d')});
        UserMessage candidate{};
        candidate.content.emplace_back(TextBlock{summary});
        ASSERT_LT(loom::services::compact::estimate_message_tokens(candidate),
                  loom::services::compact::estimate_message_tokens(historical));
        ASSERT_GT(json_string(summary).size(),json_string(std::string(3000,'d')).size());
        engine.append_message_for_testing(historical);
        const auto answer=engine.query("CURRENT_HUMAN_EXACT");
        EXPECT_FALSE(answer)<<"A token reduction alone cannot recover a payload byte failure";
        const auto requests=business_requests(server);
        ASSERT_EQ(requests.size(),1u)<<"A larger serialized request must never be retried";
        const auto after=engine.build_request_body_for_testing();
        EXPECT_EQ(after,requests.front().body)<<"Rejected payload checkpoint must not replace working history";
        EXPECT_EQ(std::ranges::count_if(engine.get_conversation(),[](const auto& message) {
            const auto* system=std::get_if<SystemMessage>(&message);
            return system && system->subtype=="compact_boundary";
        }),0);
        EXPECT_GT(server.requests().size(),requests.size())<<"The fixture must exercise a generated checkpoint candidate";
    }
}


TEST_F(WireTransport, RuntimeMaxTokensContinuationCannotReplaceOriginalHumanAnchor) {
    for(const auto protocol:{"messages","openai"}) {
        for(const bool streaming:{false,true}) {
            SCOPED_TRACE(protocol);
            SCOPED_TRACE(streaming);
            const std::string partial_text=std::string(260000,'p')+" unfinished partial response";
            TransportReply truncated;
            if(!streaming) {
                truncated=compact_reply(protocol);
                const auto text_at=truncated.body.find(json_string("wire complete"));
                ASSERT_NE(text_at,std::string::npos);
                truncated.body.replace(text_at,json_string("wire complete").size(),json_string(partial_text));
                const auto old_stop=std::string_view(protocol)=="messages"?"\"stop_reason\":\"end_turn\"":"\"finish_reason\":\"stop\"";
                const auto new_stop=std::string_view(protocol)=="messages"?"\"stop_reason\":\"max_tokens\"":"\"finish_reason\":\"length\"";
                truncated.body.replace(truncated.body.find(old_stop),std::string_view(old_stop).size(),new_stop);
            } else if(std::string_view(protocol)=="openai") {
                truncated={openai_frame("{\"choices\":[{\"delta\":{\"content\":"+json_string(partial_text)+"},\"finish_reason\":null}]}")+
                    openai_frame(R"({"choices":[{"delta":{},"finish_reason":"length"}]})")+
                    openai_frame(R"({"choices":[],"usage":{"prompt_tokens":17,"completion_tokens":9}})")+openai_frame("[DONE]")};
            } else {
                truncated={"event: message_start\ndata: {\"message\":{\"id\":\"truncated\",\"usage\":{\"input_tokens\":17}}}\n\n"
                    "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                    "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":"+json_string(partial_text)+"}}\n\n"
                    "event: content_block_stop\ndata: {\"index\":0}\n\n"
                    "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"max_tokens\"},\"usage\":{\"output_tokens\":9}}\n\n"
                    "event: message_stop\ndata: {}\n\n"};
            }
            const auto final_reply=streaming?TransportReply{std::string_view(protocol)=="openai"?openai_answer_stream():compact_messages_stream()}:compact_reply(protocol);
            WireHttpServer server({truncated,final_reply},compact_reply(protocol,true));
            ToolRegistry registry;
            auto settings=config(server,registry);
            settings.wire_api=protocol;
            settings.custom_system_prompt="Verify local work.";
            settings.context_window.auto_compact=true;
            settings.context_window.max_context_tokens=80000;
            settings.model_params.max_tokens=16384;
            QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
            const std::string human="ORIGINAL_HUMAN_ANCHOR_EXACT: LATE_CONSTRAINT_47; NEVER COMMIT; complete verification.";
            if(streaming) {
                std::vector<StreamEvent> events;
                QueryOptions options;
                options.on_event=[&](const auto& event){events.push_back(event);};
                engine.stream_query(human,options);
                EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<StreamError>(event);}),0);
            } else {
                const auto result=engine.query(human);
                ASSERT_TRUE(result)<<result.error().format();
            }
            const auto requests=business_requests(server);
            ASSERT_EQ(requests.size(),2u);
            EXPECT_GT(server.requests().size(),requests.size())<<"The continuation must trigger compaction";
            for(const auto& request:requests) {
                EXPECT_NE(request.body.find(human),std::string::npos);
                const auto body = loom::utils::json::parse(request.body);
                ASSERT_TRUE(body);
                EXPECT_EQ(body->root().get("max_tokens").as_int(), 16384);
            }
            EXPECT_EQ(engine.model_params().max_tokens, 16384u);
            const auto conversation=engine.get_conversation();
            EXPECT_EQ(std::ranges::count_if(conversation,[&](const auto& message) {
                const auto* user=std::get_if<UserMessage>(&message);
                return user && std::ranges::any_of(user->content,[&](const auto& block) {
                    const auto* text=std::get_if<TextBlock>(&block);
                    return text && text->text==human;
                });
            }),1);
        }
    }
}

TEST_F(WireTransport, AuditWriteFailurePreventsPublishingCheckpointWithUnrecoverableAnchor) {
    WireHttpServer server({compact_reply("messages")},compact_reply("messages",true));
    ToolRegistry registry;
    auto settings=config(server,registry);
    settings.wire_api="messages";
    settings.custom_system_prompt="Verify local work.";
    settings.context_window.max_context_tokens=4096;
    settings.model_params.max_tokens=128;
    settings.session_id_override="audit-write-failure";
    QueryEngine engine(settings,registry);
        isolate_compaction_system(engine);
    engine.set_session_storage(root_/"sessions");
    seed_compactable_history(engine);
    const auto audit_path=root_/"sessions"/"audit-write-failure"/"messages.jsonl";
    ASSERT_TRUE(fs::is_regular_file(audit_path));
    // Preserve the old valid audit while blocking the next append, including for root.
    fs::rename(audit_path,audit_path.string()+".previous");
    fs::create_directory(audit_path);
    UserMessage latest{};
    latest.id=MessageId{"unpersisted-human-request"};
    latest.content.emplace_back(TextBlock{"CURRENT_REQUEST_WITH_UNPERSISTED_AUDIT_ANCHOR"});
    engine.append_message_for_testing(latest);
    const auto before=engine.build_request_body_for_testing();
    const auto size=engine.get_conversation().size();
    EXPECT_FALSE(engine.compact_conversation())<<"A writable checkpoint log cannot make an absent audit anchor recoverable";
    EXPECT_EQ(engine.get_conversation().size(),size);
    EXPECT_EQ(engine.build_request_body_for_testing(),before);
    const auto checkpoint_path=root_/"sessions"/"audit-write-failure"/"context-checkpoints.jsonl";
    ASSERT_TRUE(fs::is_regular_file(checkpoint_path));
    // It is a JSONL owner-anchor log, so inspect each complete record individually.
    std::ifstream input(checkpoint_path);
    std::size_t checkpoints=0;
    for(std::string line;std::getline(input,line);) {
        const auto parsed=loom::utils::json::parse(line);
        if(parsed && parsed->root().get("kind").as_str()=="context_checkpoint") ++checkpoints;
    }
    EXPECT_EQ(checkpoints,0u);
}

TEST_F(WireTransport, SingleRestoredHumanOverflowCannotRetryAnUnchangedRequest) {
    for(const auto protocol:{"messages","openai"}) {
        SCOPED_TRACE(protocol);
        WireHttpServer server({TransportReply{R"({"error":{"message":"payload too large"}})",413,"application/json"}},compact_reply(protocol,true));
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.context_window.max_context_tokens=4096;
        settings.model_params.max_tokens=128;
        QueryEngine engine(std::move(settings),registry);
        UserMessage request{};
        request.id=MessageId{"single-restored-human"};
        const auto text=std::string(40000,'h')+" CURRENT_RESTORED_REQUEST_EXACT";
        request.content.emplace_back(TextBlock{text});
        engine.restore_conversation({Message{request}});
        std::vector<StreamEvent> events;
        QueryOptions options;
        options.resume_from_conversation=true;
        options.on_event=[&](const auto& event){events.push_back(event);};
        engine.stream_query("Do not append another request",options);
        EXPECT_EQ(business_requests(server).size(),1u);
        EXPECT_EQ(server.requests().size(),1u)<<"No reducible historical content exists";
        EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<StreamError>(event);}),1);
        const auto conversation=engine.get_conversation();
        ASSERT_EQ(conversation.size(),1u);
        const auto* retained=std::get_if<UserMessage>(&conversation.front());
        ASSERT_NE(retained,nullptr);
        EXPECT_EQ(retained->id.value,request.id.value);
        EXPECT_EQ(std::get<TextBlock>(retained->content.front()).text,text);
    }
}

TEST_F(WireTransport, UsageObservationKeepsTheActuallySentDynamicToolSnapshot) {
    for(const auto protocol:{"messages","openai"}) {
        for(const bool streaming:{false,true}) {
            SCOPED_TRACE(protocol);
            SCOPED_TRACE(streaming);
            const auto reply=streaming?TransportReply{std::string_view(protocol)=="openai"?openai_answer_stream():compact_messages_stream()}:compact_reply(protocol);
            WireHttpServer server({reply});
            ToolRegistry registry;
            auto settings=config(server,registry);
            settings.wire_api=protocol;
            settings.context_window.auto_compact=false;
            int provider_calls=0;
            settings.dynamic_tools_provider=[&] {
                ++provider_calls;
                const auto version="DYNAMIC_SCHEMA_V"+std::to_string(provider_calls);
                return std::vector<ToolDefinition>{ToolDefinition{
                    .name="DynamicSnapshotProbe",.description=version,
                    .input_schema=InputSchema{.properties={SchemaProperty{
                        .name="snapshot",.type="string",.description=version}}},
                    .permission=ToolPermission::ReadOnly}};
            };
            QueryEngine engine(std::move(settings),registry);
            isolate_compaction_system(engine);
            for(int query_number=1;query_number<=2;++query_number) {
                if(streaming) {
                    std::vector<StreamEvent> events;
                    QueryOptions options;
                    options.on_event=[&](const auto& event){events.push_back(event);};
                    engine.stream_query("Return one short answer without tools.",options);
                    EXPECT_EQ(std::ranges::count_if(events,[](const auto& event){return std::holds_alternative<StreamError>(event);}),0);
                } else {
                    const auto result=engine.query("Return one short answer without tools.");
                    ASSERT_TRUE(result)<<result.error().format();
                }
                EXPECT_EQ(provider_calls,query_number)<<"Usage observation must reuse the sent request snapshot without invoking the dynamic provider";
                const auto requests=server.requests();
                ASSERT_EQ(requests.size(),static_cast<std::size_t>(query_number));
                const auto expected="DYNAMIC_SCHEMA_V"+std::to_string(query_number);
                EXPECT_NE(requests.back().body.find(expected),std::string::npos);
                EXPECT_EQ(requests.back().body.find("DYNAMIC_SCHEMA_V"+std::to_string(query_number+1)),std::string::npos);
            }
        }
    }
}


// RFC 0026: actual large capacities distinguish model-aware budgeting from
// the old shared 200K fallback, over the real public HTTP owner boundary.
std::vector<ModelContextLimit> mixed_model_limits() {
    return {
        {.model="large-budget-model", .context_window_tokens=1000000, .max_output_tokens=64000},
        {.model="small-budget-model", .context_window_tokens=256000, .max_output_tokens=4096},
    };
}

TEST_F(WireTransport, ModelSwitchRechecksMillionAnd256KBudgetsBeforeSampling) {
    for (const auto protocol : {"messages", "openai"}) {
        for (const bool streaming : {false, true}) {
            SCOPED_TRACE(protocol);
            SCOPED_TRACE(streaming);
            auto answer=streaming
                ? TransportReply{protocol==std::string_view{"messages"} ? compact_messages_stream() : openai_answer_stream()}
                : compact_reply(protocol);
            WireHttpServer server({answer},compact_reply(protocol,true));
            ASSERT_TRUE(server.valid());
            ToolRegistry registry;
            auto settings=config(server,registry);
            settings.wire_api=protocol;
            settings.context_window.auto_compact=true;
            settings.model_params.model="large-budget-model";
            settings.model_params.max_tokens=64000;
            settings.model_context_limits=mixed_model_limits();
            QueryEngine engine(std::move(settings),registry);
            isolate_compaction_system(engine);
            seed_compactable_history(engine,600000);
            const auto sample=[&](std::string_view prompt) {
                if (!streaming) {
                    auto result=engine.query(std::string(prompt));
                    if (!result) ADD_FAILURE()<<result.error().format();
                    return result.has_value();
                }
                bool failed=false;
                QueryOptions options;
                options.on_event=[&](const auto& event) {
                    if (const auto* error=std::get_if<StreamError>(&event)) {
                        ADD_FAILURE()<<error->message;
                        failed=true;
                    }
                };
                engine.stream_query(std::string(prompt),options);
                return !failed;
            };
            EXPECT_EQ(engine.max_context_tokens(),1000000u);
            ASSERT_TRUE(sample("LARGE_CURRENT_EXACT: preserve all task constraints."));
            ASSERT_EQ(server.requests().size(),1u)<<"Large model must not use the old 200K budget";
            auto params=engine.model_params();
            params.model="small-budget-model";
            engine.set_model_params(params);
            EXPECT_EQ(engine.max_context_tokens(),256000u);
            EXPECT_EQ(engine.model_context().output_tokens,4096u);
            EXPECT_EQ(engine.model_params().max_tokens,64000u)<<"Keep the requested output reversible";
            ASSERT_TRUE(sample("SMALL_CURRENT_EXACT: preserve constraints and completed result."));
            const auto after_small=server.requests();
            ASSERT_GT(after_small.size(),2u)<<"Smaller model must summarize before its business request";
            const auto business=business_requests(server);
            ASSERT_EQ(business.size(),2u);
            EXPECT_NE(business.back().body.find("SMALL_CURRENT_EXACT"),std::string::npos);
            EXPECT_NE(business.back().body.find("LATE_CONSTRAINT_47"),std::string::npos);
            EXPECT_LT(business.back().body.size(),business.front().body.size());
            for (const auto& request : after_small) {
                if (request.body.find("CONTEXT CHECKPOINT")==std::string::npos) continue;
                auto parsed=loom::utils::json::parse(request.body);
                ASSERT_TRUE(parsed);
                EXPECT_EQ(parsed->root().get("model").as_str(),"small-budget-model");
                EXPECT_LE(parsed->root().get("max_tokens").as_int(),4096);
            }
            params=engine.model_params();
            params.model="large-budget-model";
            engine.set_model_params(params);
            EXPECT_EQ(engine.max_context_tokens(),1000000u);
            EXPECT_EQ(engine.model_context().output_tokens,64000u);
            ASSERT_TRUE(sample("LARGE_RETURN_EXACT: finish using the checkpoint."));
            const auto final_business=business_requests(server);
            ASSERT_EQ(final_business.size(),3u);
            const std::array<std::string_view,3> models{"large-budget-model","small-budget-model","large-budget-model"};
            const std::array<std::int64_t,3> outputs{64000,4096,64000};
            for (std::size_t i=0;i<3;++i) {
                auto parsed=loom::utils::json::parse(final_business[i].body);
                ASSERT_TRUE(parsed);
                EXPECT_EQ(parsed->root().get("model").as_str(),models[i]);
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(),outputs[i]);
                EXPECT_EQ(final_business[i].authorization,"Bearer transport-bearer");
            }
        }
    }
}

TEST_F(WireTransport, ModelFallbackCompactsFor256KBeforeRetryingOverloadedMillionModel) {
    for (const auto protocol : {"messages","openai"}) {
        SCOPED_TRACE(protocol);
        WireHttpServer server({{R"({"error":{"type":"overloaded_error","message":"Overloaded"}})",529,"application/json"},compact_reply(protocol)},compact_reply(protocol,true));
        ASSERT_TRUE(server.valid());
        ToolRegistry registry;
        auto settings=config(server,registry);
        settings.wire_api=protocol;
        settings.context_window.auto_compact=true;
        settings.retry_policy.max_retries=1;
        settings.model_params.model="large-budget-model";
        settings.model_params.max_tokens=64000;
        settings.model_context_limits=mixed_model_limits();
        settings.fallback_models={"small-budget-model"};
        QueryEngine engine(std::move(settings),registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine,600000);
        const auto result=engine.query("FALLBACK_CURRENT_EXACT: preserve the original task and result.");
        ASSERT_TRUE(result)<<result.error().format();
        EXPECT_EQ(engine.max_context_tokens(),256000u);
        EXPECT_EQ(engine.model_context().output_tokens,4096u);
        const auto requests=server.requests();
        ASSERT_GT(requests.size(),2u);
        EXPECT_EQ(requests.front().body.find("CONTEXT CHECKPOINT"),std::string::npos);
        EXPECT_NE(requests[1].body.find("CONTEXT CHECKPOINT"),std::string::npos);
        const auto business=business_requests(server);
        ASSERT_EQ(business.size(),2u);
        for (std::size_t i=0;i<business.size();++i) {
            const auto parsed=loom::utils::json::parse(business[i].body);
            ASSERT_TRUE(parsed);
            EXPECT_EQ(parsed->root().get("model").as_str(),i==0 ? "large-budget-model" : "small-budget-model");
            EXPECT_EQ(parsed->root().get("max_tokens").as_int(),i==0 ? 64000 : 4096);
        }
        EXPECT_LT(business.back().body.size(),business.front().body.size());
        EXPECT_NE(business.back().body.find("FALLBACK_CURRENT_EXACT"),std::string::npos);
        EXPECT_NE(business.back().body.find("LATE_CONSTRAINT_47"),std::string::npos);
    }
}

TEST_F(WireTransport, ModelContextAssemblyKeepsOriginalFallbackAndIndependentCatalogOutputCap) {
    loom::core::Settings settings;
    settings.model.default_model="gpt-4.1";
    settings.model.context_window_size=123456;
    settings.model.max_output_tokens=64000;
    settings.model.context_limits={{.model="gpt-4.1",.context_window_tokens=256000}};
    loom::query::AssemblyOverrides overrides;
    overrides.base_url="https://api.openai.com/v1";
    overrides.wire_api="openai";
    overrides.cwd=root_.string();
    auto resolved=loom::query::resolve_engine_config(settings,overrides);
    ASSERT_TRUE(resolved);
    // This is a catalog-only regression. Freeze an empty attempted discovery
    // for each synthetic external identity rather than contacting real services.
    const auto no_discovery = [](QueryEngineConfig& config) {
        auto snapshot = std::make_shared<QueryEngineConfig::ModelMetadataSnapshot>();
        snapshot->base_url = config.base_url.value_or("");
        snapshot->wire_api = config.wire_api.value_or("messages");
        snapshot->api_key = config.api_key;
        snapshot->auth_token = config.auth_token;
        config.model_metadata = std::move(snapshot);
    };
    no_discovery(*resolved);
    ToolRegistry registry;
    QueryEngine engine(*resolved,registry);
    EXPECT_EQ(engine.max_context_tokens(),256000u);
    EXPECT_EQ(engine.model_context().source,"profile");
    EXPECT_EQ(engine.model_context().output_tokens,32768u);
    EXPECT_EQ(engine.model_context().output_cap_source,"catalog");
    const auto snapshot=make_api_execution_context(*resolved);
    ASSERT_TRUE(snapshot->model_context_config);
    EXPECT_EQ(snapshot->model_context_config->fallback_window_tokens,123456u);
    EXPECT_EQ(snapshot->context_output_tokens,64000u);
    auto params=engine.model_params();
    params.model="unknown-private-alias";
    engine.set_model_params(params);
    EXPECT_EQ(engine.max_context_tokens(),123456u);
    EXPECT_EQ(engine.model_context().source,"fallback");
    EXPECT_EQ(engine.model_context().output_tokens,64000u);
    params.model="gpt-4.1";
    engine.set_model_params(params);
    EXPECT_EQ(engine.model_context().output_tokens,32768u);
    auto catalog_config=*resolved;
    catalog_config.model_context_limits.clear();
    QueryEngine automatic(catalog_config,registry);
    EXPECT_EQ(automatic.max_context_tokens(),1047576u);
    EXPECT_EQ(automatic.model_context().source,"catalog");
    EXPECT_EQ(automatic.model_context().output_tokens,32768u);
    catalog_config.base_url="https://private-gateway.invalid/v1";
    no_discovery(catalog_config);
    QueryEngine private_alias(catalog_config,registry);
    EXPECT_EQ(private_alias.max_context_tokens(),123456u);
    EXPECT_EQ(private_alias.model_context().source,"fallback");
    EXPECT_EQ(private_alias.model_context().output_tokens,64000u);
}


TEST_F(WireTransport, ModelOutputCapRejectsImpossibleForcedThinkingBeforeHttpAndRestoresOnSwitch) {
    for (const auto protocol : {"messages","openai"}) {
        for (const bool streaming : {false,true}) {
            for (const bool auto_compact : {false,true}) {
                SCOPED_TRACE(protocol);
                SCOPED_TRACE(streaming);
                SCOPED_TRACE(auto_compact);
                auto answer=streaming
                    ? TransportReply{protocol==std::string_view{"messages"} ? compact_messages_stream() : openai_answer_stream()}
                    : compact_reply(protocol);
                WireHttpServer server({answer},compact_reply(protocol,true));
                ASSERT_TRUE(server.valid());
                ToolRegistry registry;
                auto settings=config(server,registry);
                settings.wire_api=protocol;
                settings.context_window.auto_compact=auto_compact;
                settings.model_params.model="large-budget-model";
                settings.model_params.max_tokens=64000;
                settings.model_context_limits={
                    {.model="large-budget-model",.context_window_tokens=1000000,.max_output_tokens=64000},
                    {.model="small-budget-model",.context_window_tokens=256000,.max_output_tokens=8192},
                    {.model="equal-thinking-budget",.context_window_tokens=256000,.max_output_tokens=32000}};
                settings.thinking_config.mode=ThinkingConfig::Mode::Forced;
                settings.thinking_config.budget_tokens=32000;
                QueryEngine engine(std::move(settings),registry);
                isolate_compaction_system(engine);
                const auto sample=[&] {
                    if (!streaming) {
                        auto result=engine.query("Finish without tools; preserve the configured thinking request.");
                        if (!result) {
                            EXPECT_EQ(result.error().code,ErrorCode::InvalidRequest);
                            EXPECT_NE(result.error().message.find("Forced thinking budget 32000"),std::string::npos);
                        }
                        return result.has_value();
                    }
                    bool failed=false;
                    QueryOptions options;
                    options.on_event=[&](const auto& event) {
                        if (const auto* error=std::get_if<StreamError>(&event)) {
                            EXPECT_NE(error->message.find("Forced thinking budget 32000"),std::string::npos);
                            failed=true;
                        }
                    };
                    engine.stream_query("Finish without tools; preserve configured thinking.",options);
                    return !failed;
                };
                ASSERT_TRUE(sample());
                ASSERT_EQ(server.requests().size(),1u);
                for (const auto model : {"small-budget-model","equal-thinking-budget"}) {
                    auto params=engine.model_params();
                    params.model=model;
                    engine.set_model_params(params);
                    const bool accepted=sample();
                    EXPECT_EQ(accepted,protocol==std::string_view{"openai"});
                }
                EXPECT_EQ(server.requests().size(),protocol==std::string_view{"messages"} ? 1u : 3u)
                    << "Invalid manual thinking must not send a summary or business HTTP request";
                auto params=engine.model_params();
                params.model="large-budget-model";
                engine.set_model_params(params);
                ASSERT_TRUE(sample());
                const auto requests=server.requests();
                ASSERT_EQ(requests.size(),protocol==std::string_view{"messages"} ? 2u : 4u);
                for (const auto& request : requests) {
                    const auto parsed=loom::utils::json::parse(request.body);
                    ASSERT_TRUE(parsed);
                    if (protocol==std::string_view{"messages"}) {
                        EXPECT_EQ(parsed->root().get("max_tokens").as_int(),64000);
                        EXPECT_EQ(parsed->root().get("thinking").get("budget_tokens").as_int(),32000);
                    } else EXPECT_FALSE(parsed->root().get("thinking").valid());
                }
            }
        }
    }
}


TEST_F(WireTransport, ModelOutputCapPreservesAdaptiveAndDisabledThinkingWithUnusedManualBudget) {
    for (const auto protocol : {"messages","openai"}) {
        for (const auto mode : {ThinkingConfig::Mode::Adaptive,ThinkingConfig::Mode::Disabled}) {
            for (const bool streaming : {false,true}) {
                SCOPED_TRACE(protocol);
                SCOPED_TRACE(static_cast<int>(mode));
                SCOPED_TRACE(streaming);
                auto answer=streaming
                    ? TransportReply{protocol==std::string_view{"messages"} ? compact_messages_stream() : openai_answer_stream()}
                    : compact_reply(protocol);
                WireHttpServer server({answer});
                ASSERT_TRUE(server.valid());
                ToolRegistry registry;
                auto settings=config(server,registry);
                settings.wire_api=protocol;
                settings.model_params.model="small-budget-model";
                settings.model_params.max_tokens=64000;
                settings.model_context_limits={{.model="small-budget-model",.context_window_tokens=256000,.max_output_tokens=8192}};
                settings.thinking_config.mode=mode;
                settings.thinking_config.budget_tokens=32000;
                QueryEngine engine(std::move(settings),registry);
                isolate_compaction_system(engine);
                if (streaming) {
                    QueryOptions options;
                    options.on_event=[](const auto& event) {
                        if (const auto* error=std::get_if<StreamError>(&event)) ADD_FAILURE()<<error->message;
                    };
                    engine.stream_query("Finish the bounded verification.",options);
                } else {
                    const auto answer_result=engine.query("Finish the bounded verification.");
                    ASSERT_TRUE(answer_result)<<answer_result.error().format();
                }
                const auto requests=server.requests();
                ASSERT_EQ(requests.size(),1u);
                const auto parsed=loom::utils::json::parse(requests.front().body);
                ASSERT_TRUE(parsed);
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(),8192);
                const auto thinking=parsed->root().get("thinking");
                if (protocol==std::string_view{"messages"} && mode==ThinkingConfig::Mode::Adaptive) {
                    EXPECT_EQ(thinking.get("type").as_str(),"adaptive");
                    EXPECT_FALSE(thinking.get("budget_tokens").valid());
                } else EXPECT_FALSE(thinking.valid());
            }
        }
    }
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

namespace {
TransportReply occupancy_reply(std::string_view protocol, bool streaming, TokenUsage usage,
    bool has_usage = true, bool checkpoint = false) {
    loom::utils::json::JsonMutDoc doc;
    auto values = doc.object();
    values.add(protocol == "messages" ? "input_tokens" : "prompt_tokens", doc.number(static_cast<int64_t>(usage.input_tokens)));
    values.add(protocol == "messages" ? "output_tokens" : "completion_tokens", doc.number(static_cast<int64_t>(usage.output_tokens)));
    if (protocol == "messages") {
        values.add("cache_creation_input_tokens", doc.number(static_cast<int64_t>(usage.cache_creation_tokens)));
        values.add("cache_read_input_tokens", doc.number(static_cast<int64_t>(usage.cache_read_tokens)));
    } else {
        auto details = doc.object();
        details.add("cached_tokens", doc.number(static_cast<int64_t>(usage.cache_read_tokens)));
        values.add("prompt_tokens_details", details);
    }
    doc.set_root(values);
    const auto usage_json = doc.to_string();
    if (!streaming) {
        auto reply = compact_reply(protocol, checkpoint);
        auto parsed = loom::utils::json::parse(reply.body);
        loom::utils::json::JsonMutDoc response;
        auto root = response.copy_val(parsed->root());
        (void)root.remove("usage");
        if (has_usage) root.add("usage", response.raw_json(usage_json));
        response.set_root(root);
        reply.body = response.to_string();
        return reply;
    }
    const auto text = json_string(checkpoint ? checkpoint_text() : "wire complete");
    if (protocol == "openai") return {
        openai_frame("{\"choices\":[{\"delta\":{\"content\":" + text + "},\"finish_reason\":\"stop\"}]}") +
        (has_usage ? openai_frame("{\"choices\":[],\"usage\":" + usage_json + "}") : std::string{}) + openai_frame("[DONE]")};
    return {"event: message_start\ndata: {\"message\":{\"id\":\"occupancy\"" +
        (has_usage ? ",\"usage\":" + usage_json : std::string{}) + "}}\n\n"
        "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
        "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":" + text + "}}\n\n"
        "event: content_block_stop\ndata: {\"index\":0}\n\n"
        "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"end_turn\"}" +
        (has_usage ? ",\"usage\":" + usage_json : std::string{}) + "}\n\n"
        "event: message_stop\ndata: {}\n\n"};
}

bool occupancy_turn(QueryEngine& engine, bool streaming, std::string_view prompt = "Continue the context probe",
    QueryOptions options = {}) {
    if (!streaming) return engine.query(prompt, options).has_value();
    bool ended = false;
    bool failed = false;
    options.on_event = [&](const StreamEvent& event) {
        ended |= std::holds_alternative<StreamEnd>(event);
        failed |= std::holds_alternative<StreamError>(event);
    };
    engine.stream_query(prompt, options);
    return ended && !failed;
}

TEST_F(WireTransport, CurrentContextUsesLatestBusinessInputInsteadOfAccumulatedBilling) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {5000, 7}),
            occupancy_reply(protocol, streaming, {5100, 9})});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        auto first = engine.context_usage();
        ASSERT_TRUE(first.last_request_usage);
        EXPECT_EQ(first.last_request_usage->input_tokens, 5000u);
        EXPECT_GE(first.tokens, 5000u);
        EXPECT_LT(first.tokens, 5100u);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        const auto second = engine.context_usage();
        ASSERT_TRUE(second.last_request_usage);
        EXPECT_EQ(second.last_request_usage->input_tokens, 5100u);
        EXPECT_GE(second.tokens, 5100u);
        EXPECT_LT(second.tokens, 5200u);
        EXPECT_EQ(engine.get_usage().input_tokens, 10100u);
        EXPECT_EQ(engine.get_usage().output_tokens, 16u);
        EXPECT_DOUBLE_EQ(engine.context_utilization(), static_cast<double>(second.tokens) / second.window_tokens);
        const auto count = server.requests().size();
        AssistantMessage retained;
        retained.id.value = "new-retained-output";
        retained.content.emplace_back(TextBlock{std::string(4000, 'a')});
        engine.append_message_for_testing(Message{std::move(retained)});
        EXPECT_GE(engine.context_usage().tokens, second.tokens + 1000);
        EXPECT_EQ(server.requests().size(), count);
        EXPECT_EQ(engine.get_usage().input_tokens, 10100u);
    }
}

TEST_F(WireTransport, CurrentContextCountsCachedInputOnceForBothWireProtocols) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        const TokenUsage usage = std::string_view(protocol) == "messages" ? TokenUsage{1000, 7, 2000, 3000}
            : TokenUsage{6000, 7, 0, 3000};
        WireHttpServer server({occupancy_reply(protocol, streaming, usage)});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        const auto current = engine.context_usage();
        ASSERT_TRUE(current.last_request_usage);
        EXPECT_GE(current.tokens, 6000u);
        EXPECT_LT(current.tokens, 6100u);
        EXPECT_EQ(current.last_request_usage->input_tokens, usage.input_tokens);
        EXPECT_EQ(current.last_request_usage->cache_read_tokens, std::string_view(protocol) == "messages" ? 3000u : 0u);
        EXPECT_EQ(current.last_request_usage->cache_creation_tokens, std::string_view(protocol) == "messages" ? 2000u : 0u);
    }
}

TEST_F(WireTransport, CurrentContextKeepsActualDynamicAndVerbatimSchemasWithoutGetterProviderCalls) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {17, 9})});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        int dynamic_calls = 0;
        int mcp_calls = 0;
        settings.dynamic_tools_provider = [&] {
            ++dynamic_calls;
            return std::vector<ToolDefinition>{{.name = "DynamicContextProbe", .description = "dynamic tool"}};
        };
        settings.mcp_input_schema_provider = [&] {
            ++mcp_calls;
            return std::unordered_map<std::string, std::string>{{"DynamicContextProbe",
                R"({"type":"object","description":")" + std::string(20000, 'v') + R"(","properties":{}})"}};
        };
        settings.tools = {{.name = "DeniedContextTool", .description = std::string(100000, 'd')}};
        settings.always_deny_rules = {"DeniedContextTool"};
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        const auto initial = engine.context_usage();
        EXPECT_LT(initial.tokens, 1000u);
        EXPECT_EQ(dynamic_calls, 0);
        EXPECT_EQ(mcp_calls, 0);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        const auto sampled = engine.context_usage();
        EXPECT_GT(sampled.tokens, initial.tokens + 4000);
        EXPECT_EQ(dynamic_calls, 1);
        EXPECT_EQ(mcp_calls, 1);
        for (int read = 0; read < 100; ++read) {
            EXPECT_EQ(engine.context_usage().tokens, sampled.tokens);
            EXPECT_DOUBLE_EQ(engine.context_utilization(), static_cast<double>(sampled.tokens) / sampled.window_tokens);
        }
        EXPECT_EQ(dynamic_calls, 1);
        EXPECT_EQ(mcp_calls, 1);
        ASSERT_EQ(server.requests().size(), 1u);
        EXPECT_EQ(server.requests().front().body.find("DeniedContextTool"), std::string::npos);
        EXPECT_NE(server.requests().front().body.find(std::string(1000, 'v')), std::string::npos);
    }
}

TEST_F(WireTransport, CurrentContextReadsNeverPrepareAnInjectedBackendOrSendHttp) {
    for (const bool streaming : {false, true}) {
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply("openai", streaming, {5000, 7})});
        ToolRegistry registry;
        QueryEngine engine(config(server, registry), registry);
        isolate_compaction_system(engine);
        auto prepares = std::make_shared<std::atomic<int>>(0);
        engine.set_wire_backend_factory([prepares, url = server.base_url()] {
            return std::make_unique<CustomTransportBackend>(url, prepares);
        });
        const auto initial_prepares = prepares->load();
        EXPECT_EQ(initial_prepares, 0);
        for (int read = 0; read < 100; ++read) (void)engine.context_usage();
        EXPECT_EQ(prepares->load(), initial_prepares);
        EXPECT_TRUE(server.requests().empty());
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        const auto sampled_prepares = prepares->load();
        for (int read = 0; read < 100; ++read) {
            (void)engine.context_usage();
            (void)engine.context_utilization();
        }
        EXPECT_EQ(prepares->load(), sampled_prepares);
        EXPECT_EQ(sampled_prepares, initial_prepares + 1);
        EXPECT_EQ(server.requests().size(), 1u);
        engine.set_wire_backend_factory([prepares, url = server.base_url()] {
            return std::make_unique<CustomTransportBackend>(url, prepares);
        });
        EXPECT_FALSE(engine.context_usage().last_request_usage);
        EXPECT_LT(engine.context_usage().tokens, 1000u);
        EXPECT_EQ(prepares->load(), sampled_prepares);
        EXPECT_EQ(server.requests().size(), 1u);
    }
}

TEST_F(WireTransport, CurrentContextCompactionRestoreAndClearDiscardOldUsageFloors) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {6000, 7})},
            occupancy_reply(protocol, false, {12000, 13}, true, true));
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        settings.context_window.max_context_tokens = 4096;
        settings.model_params.max_tokens = 128;
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        seed_compactable_history(engine);
        const auto before = engine.context_usage();
        ASSERT_TRUE(before.last_request_usage);
        ASSERT_TRUE(engine.compact_conversation());
        const auto compacted = engine.context_usage();
        EXPECT_LT(compacted.tokens, before.tokens);
        EXPECT_FALSE(compacted.last_request_usage);
        EXPECT_GT(engine.get_usage().input_tokens, before.last_request_usage->input_tokens);
        EXPECT_GT(server.requests().size(), business_requests(server).size());
        auto retained = engine.get_conversation();
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        ASSERT_TRUE(engine.context_usage().last_request_usage);
        const auto billed = engine.get_usage().input_tokens;
        engine.restore_conversation(std::move(retained));
        EXPECT_EQ(engine.context_usage().tokens, compacted.tokens);
        EXPECT_FALSE(engine.context_usage().last_request_usage);
        EXPECT_EQ(engine.get_usage().input_tokens, billed);
        engine.clear_conversation();
        const auto cleared = engine.context_usage();
        EXPECT_GT(cleared.tokens, 0u) << "A fresh working context still contains system instructions";
        EXPECT_LT(cleared.tokens, before.tokens);
        EXPECT_FALSE(cleared.last_request_usage);
        EXPECT_EQ(engine.get_usage().input_tokens, billed);
    }
}

TEST_F(WireTransport, CurrentContextFailuresSummaryAndMissingUsageDoNotInventBusinessMetrics) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        ToolRegistry registry;
        {
            WireHttpServer server({occupancy_reply(protocol, streaming, {6000, 7}),
                {R"({"error":{"type":"authentication_error","message":"denied"}})", 401, "application/json"}});
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            ASSERT_TRUE(occupancy_turn(engine, streaming));
            EXPECT_FALSE(occupancy_turn(engine, streaming));
            const auto failed = engine.context_usage();
            ASSERT_TRUE(failed.last_request_usage);
            EXPECT_EQ(failed.last_request_usage->input_tokens, 6000u);
            EXPECT_EQ(engine.get_usage().input_tokens, 6000u);
        }
        {
            WireHttpServer server({occupancy_reply(protocol, streaming, {6000, 7}),
                occupancy_reply(protocol, streaming, {}, false)});
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            ASSERT_TRUE(occupancy_turn(engine, streaming));
            ASSERT_TRUE(engine.context_usage().last_request_usage);
            ASSERT_TRUE(occupancy_turn(engine, streaming));
            EXPECT_FALSE(engine.context_usage().last_request_usage);
            EXPECT_EQ(engine.get_usage().input_tokens, 6000u);
            EXPECT_GE(engine.context_usage().tokens, 6000u);
        }
        {
            WireHttpServer server({occupancy_reply(protocol, streaming, {6000, 7})},
                occupancy_reply(protocol, false, {12000, 13})); // A complete response with invalid summary sections.
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            settings.context_window.max_context_tokens = 4096;
            settings.model_params.max_tokens = 128;
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            ASSERT_TRUE(occupancy_turn(engine, streaming));
            seed_compactable_history(engine);
            const auto before = engine.context_usage();
            EXPECT_FALSE(engine.compact_conversation());
            const auto rejected = engine.context_usage();
            EXPECT_EQ(rejected.tokens, before.tokens);
            ASSERT_TRUE(rejected.last_request_usage);
            EXPECT_EQ(rejected.last_request_usage->input_tokens, 6000u);
            EXPECT_EQ(engine.get_usage().input_tokens, 18000u);
        }
    }
}

TEST_F(WireTransport, CurrentContextSharesBusinessProjectionForSnipBoundariesAndRetainedTools) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {}, false)});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        settings.context_window.max_context_tokens = 1000;
        settings.model_params.max_tokens = 128;
        QueryEngine engine(std::move(settings), registry);
        SystemMessage system;
        system.id.value = "projection-system";
        system.content.emplace_back(TextBlock{"Controlled system instructions"});
        UserMessage removed;
        removed.id.value = "projection-removed";
        removed.content.emplace_back(TextBlock{"REMOVED_CONTEXT_" + std::string(20000, 'r')});
        SystemMessage compact;
        compact.id.value = "projection-compact";
        compact.subtype = "compact_boundary";
        compact.content.emplace_back(TextBlock{"REMOVED_BOUNDARY_" + std::string(20000, 'c')});
        SystemMessage snip;
        snip.id.value = "projection-snip";
        snip.subtype = "snip_boundary";
        snip.snip_metadata = SnipMetadata{{removed.id.value}};
        ToolUseMessage call;
        call.id.value = "projection-call-message";
        call.tool_name = "Echo";
        call.tool_input_json = R"({"text":"retain"})";
        call.content.emplace_back(ToolUseBlock{.id = ToolUseId{"projection-call"}, .name = "Echo", .input_json = call.tool_input_json});
        ToolResultMessage result;
        result.id.value = "projection-result";
        result.tool_use_id.value = "projection-call";
        result.tool_name = "Echo";
        result.content.emplace_back(TextBlock{"RETAINED_TOOL_OUTPUT_" + std::string(8000, 't')});
        UserMessage current;
        current.id.value = "projection-current";
        current.content.emplace_back(TextBlock{"Continue from the retained tool observation"});
        const std::vector<Message> visible{Message{call}, Message{result}, Message{current}};
        const auto expected = loom::services::compact::estimate_request_tokens({visible,
            "Controlled system instructions", "", {}, {}}, false);
        engine.restore_conversation({Message{system}, Message{removed}, Message{compact}, Message{snip},
            Message{call}, Message{result}, Message{current}});
        const auto before = engine.context_usage();
        EXPECT_EQ(before.tokens, expected);
        EXPECT_GT(before.tokens, before.window_tokens);
        EXPECT_GT(engine.context_utilization(), 1.0);
        EXPECT_EQ(engine.get_conversation().size(), 7u) << "The getter must not mutate or replay audit history";
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        ASSERT_EQ(server.requests().size(), 1u);
        const auto body = server.requests().front().body;
        EXPECT_EQ(body.find("REMOVED_CONTEXT_"), std::string::npos);
        EXPECT_EQ(body.find("REMOVED_BOUNDARY_"), std::string::npos);
        EXPECT_NE(body.find("RETAINED_TOOL_OUTPUT_"), std::string::npos);
        EXPECT_GT(engine.context_usage().tokens, before.tokens);
        EXPECT_FALSE(engine.context_usage().last_request_usage);
    }
}

TEST_F(WireTransport, CurrentContextSnipImmediatelyInvalidatesTheOldMeasuredFloor) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {20000, 7})});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        UserMessage removed;
        removed.id.value = "snip-large-observation";
        removed.content.emplace_back(TextBlock{std::string(12000, 'h')});
        engine.append_message_for_testing(Message{removed});
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        EXPECT_GE(engine.context_usage().tokens, 20000u);
        ASSERT_TRUE(engine.context_usage().last_request_usage);
        SystemMessage snip;
        snip.id.value = "current-occupancy-snip";
        snip.subtype = "snip_boundary";
        snip.snip_metadata = SnipMetadata{{removed.id.value}};
        engine.append_message_for_testing(Message{snip});
        EXPECT_LT(engine.context_usage().tokens, 1000u);
        EXPECT_FALSE(engine.context_usage().last_request_usage);
        EXPECT_EQ(engine.get_usage().input_tokens, 20000u);
        EXPECT_EQ(server.requests().size(), 1u);
    }
}

TEST_F(WireTransport, CurrentContextModelSwitchChangesWindowAndClearsUsageWithoutHttp) {
    for (const auto protocol : {"messages", "openai"}) for (const bool streaming : {false, true}) {
        SCOPED_TRACE(protocol);
        SCOPED_TRACE(streaming);
        WireHttpServer server({occupancy_reply(protocol, streaming, {5000, 7})});
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        settings.model_params.model = "occupancy-large";
        settings.model_context_limits = {{.model = "occupancy-large", .context_window_tokens = 1000000},
            {.model = "occupancy-small", .context_window_tokens = 256000}};
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        ASSERT_TRUE(occupancy_turn(engine, streaming));
        const auto measured = engine.context_usage();
        ASSERT_TRUE(measured.last_request_usage);
        engine.set_session_id("same-working-context-new-id");
        EXPECT_EQ(engine.context_usage().tokens, measured.tokens);
        EXPECT_TRUE(engine.context_usage().last_request_usage);
        auto params = engine.model_params();
        params.model = "occupancy-small";
        engine.set_model_params(params);
        const auto small = engine.context_usage();
        EXPECT_EQ(small.window_tokens, 256000u);
        EXPECT_LT(small.tokens, measured.tokens);
        EXPECT_FALSE(small.last_request_usage);
        EXPECT_DOUBLE_EQ(engine.context_utilization(), static_cast<double>(small.tokens) / small.window_tokens);
        params.model = "occupancy-large";
        engine.set_model_params(params);
        const auto large = engine.context_usage();
        EXPECT_EQ(large.window_tokens, 1000000u);
        EXPECT_EQ(large.tokens, small.tokens);
        EXPECT_FALSE(large.last_request_usage);
        EXPECT_EQ(server.requests().size(), 1u);
        EXPECT_EQ(engine.get_usage().input_tokens, 5000u);
    }
}
} // namespace


TEST_F(WireTransport, AutomaticModelSwitchUsesProviderDefaultsWithoutLosingRawPolicy) {
    for (const auto protocol : {"messages", "openai"}) {
        for (const bool streaming : {false, true}) {
            SCOPED_TRACE(protocol);
            SCOPED_TRACE(streaming);
            const auto answer = streaming ? TransportReply{std::string_view(protocol) == "messages"
                ? compact_messages_stream() : openai_answer_stream()} : compact_reply(protocol);
            WireHttpServer server({answer});
            ASSERT_TRUE(server.valid());
            ToolRegistry registry;
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            settings.model_params.model = "large-budget-model";
            settings.model_context_limits = mixed_model_limits();
            settings.thinking_config = {};
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            for (const auto model : {"large-budget-model", "small-budget-model", "private-alias", "large-budget-model"}) {
                auto params = engine.model_params();
                params.model = model;
                engine.set_model_params(params);
                if (streaming) {
                    bool failed = false;
                    QueryOptions options;
                    options.on_event = [&](const auto& event) { if (std::holds_alternative<StreamError>(event)) failed = true; };
                    engine.stream_query("Finish without tools.", options);
                    ASSERT_FALSE(failed);
                } else {
                    const auto result = engine.query("Finish without tools.");
                    ASSERT_TRUE(result) << result.error().format();
                }
                EXPECT_EQ(engine.model_params().max_tokens, 0u);
            }
            const auto requests = server.requests();
            ASSERT_EQ(requests.size(), 4u);
            const std::array<std::uint32_t, 4> reserves{64000, 4096, 16384, 64000};
            for (std::size_t index = 0; index < requests.size(); ++index) {
                const auto body = loom::utils::json::parse(requests[index].body);
                ASSERT_TRUE(body);
                const auto output = body->root().get("max_tokens");
                if (std::string_view(protocol) == "openai") EXPECT_FALSE(output.valid());
                else EXPECT_EQ(output.as_int(), reserves[index]);
                EXPECT_FALSE(body->root().get("thinking").valid());
                EXPECT_FALSE(body->root().get("temperature").valid());
            }
        }
    }
}

TEST_F(WireTransport, ThinkingOverridesUseOneRecipeAndValidateAutomaticOutputBeforeHttp) {
    for (const auto enabled : {std::optional<bool>{}, std::optional<bool>{true}, std::optional<bool>{false}}) {
        for (const bool streaming : {false, true}) {
            for (const auto budget : {32768u, std::numeric_limits<std::uint32_t>::max()}) {
                WireHttpServer server({streaming ? TransportReply{compact_messages_stream()} : compact_reply("messages")});
                ASSERT_TRUE(server.valid());
                ToolRegistry registry;
                SCOPED_TRACE(std::format("thinking={}, budget={}, streaming={}",
                    enabled ? (*enabled ? "true" : "false") : "unset", budget, streaming));
                Settings settings;
                settings.model.default_model = "private-model";
                settings.network.api_key = "transport-api-key";
                settings.model.extended_thinking = enabled;
                settings.model.thinking_budget = budget;
                settings.model.temperature = 0.9;
                const auto resolved = loom::query::resolve_engine_config(settings,
                    {.base_url = server.base_url(), .wire_api = "messages", .cwd = root_.string()});
                ASSERT_TRUE(resolved);
                EXPECT_EQ(resolved->thinking_config.mode,
                    enabled == false ? ThinkingConfig::Mode::Disabled : ThinkingConfig::Mode::Forced);
                EXPECT_EQ(resolved->model_params.temperature, std::optional<double>{0.9});
                auto config = *resolved;
                config.native_computer_tool = false;
                QueryEngine engine(std::move(config), registry);
                isolate_compaction_system(engine);
                const bool valid = enabled == false || budget == 32768;
                if (streaming) {
                    bool failed = false;
                    QueryOptions options;
                    options.on_event = [&](const auto& event) { if (std::holds_alternative<StreamError>(event)) failed = true; };
                    engine.stream_query("Finish without tools.", options);
                    EXPECT_EQ(!failed, valid);
                } else {
                    const auto result = engine.query("Finish without tools.");
                    EXPECT_EQ(result.has_value(), valid) << (result ? "" : result.error().format());
                }
                const auto requests = server.requests();
                ASSERT_EQ(requests.size(), valid ? 1u : 0u);
                if (!valid) continue;
                const auto body = loom::utils::json::parse(requests.front().body);
                ASSERT_TRUE(body);
                if (enabled == false) EXPECT_FALSE(body->root().get("thinking").valid());
                else {
                    EXPECT_EQ(body->root().get("max_tokens").as_int(), 32769);
                    EXPECT_EQ(body->root().get("thinking").get("budget_tokens").as_int(), 32768);
                }
            }
        }
    }
    EXPECT_EQ(resolve_thinking_config({}, {}).mode, ThinkingConfig::Mode::Default);
    EXPECT_EQ(resolve_thinking_config(true, {}).mode, ThinkingConfig::Mode::Adaptive);
}

TEST_F(WireTransport, AutomaticOutputContinuationKeepsProviderDefault) {
    for (const auto protocol : {"messages", "openai"}) {
        for (const bool streaming : {false, true}) {
            TransportReply truncated = streaming ? TransportReply{std::string_view(protocol) == "messages"
                ? compact_messages_stream() : openai_answer_stream()} : compact_reply(protocol);
            const auto stop = std::string_view(protocol) == "messages" ? "end_turn" : "stop";
            const auto limit = std::string_view(protocol) == "messages" ? "max_tokens" : "length";
            const auto position = truncated.body.find(std::string("\"") + stop + "\"");
            ASSERT_NE(position, std::string::npos);
            truncated.body.replace(position, std::string_view(stop).size() + 2, std::string("\"") + limit + "\"");
            const auto answer = streaming ? TransportReply{std::string_view(protocol) == "messages"
                ? compact_messages_stream() : openai_answer_stream()} : compact_reply(protocol);
            WireHttpServer server({truncated, answer});
            ToolRegistry registry;
            auto settings = config(server, registry);
            settings.wire_api = protocol;
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            if (streaming) {
                QueryOptions options;
                options.on_event = [](const auto&) {};
                engine.stream_query("Finish without tools.", options);
            } else ASSERT_TRUE(engine.query("Finish without tools."));
            const auto requests = server.requests();
            ASSERT_EQ(requests.size(), 2u);
            for (const auto& request : requests) {
                const auto body = loom::utils::json::parse(request.body);
                ASSERT_TRUE(body);
                if (std::string_view(protocol) == "openai") EXPECT_FALSE(body->root().get("max_tokens").valid());
                else EXPECT_EQ(body->root().get("max_tokens").as_int(), 16384);
            }
            EXPECT_EQ(engine.model_params().max_tokens, 0u);
        }
    }
}


TEST_F(WireTransport, AutomaticManualThinkingRejectsCapacityAndHeadroomConflictsBeforeHttp) {
    for (const auto& profile : {ModelContextLimit{.model="private", .context_window_tokens=200000, .max_output_tokens=32768},
                               ModelContextLimit{.model="private", .context_window_tokens=1000}}) {
        for (const bool streaming : {false, true}) {
            WireHttpServer server({compact_reply("messages")});
            ToolRegistry registry;
            auto settings = config(server, registry);
            settings.wire_api = "messages";
            settings.model_params.model = "private";
            settings.model_context_limits = {profile};
            settings.thinking_config = resolve_thinking_config(true, 32768);
            QueryEngine engine(std::move(settings), registry);
            isolate_compaction_system(engine);
            if (streaming) {
                bool failed = false;
                QueryOptions options;
                options.on_event = [&](const auto& event) { if (std::holds_alternative<StreamError>(event)) failed = true; };
                engine.stream_query("Complete once.", options);
                EXPECT_TRUE(failed);
            } else {
                const auto result = engine.query("Complete once.");
                ASSERT_FALSE(result);
                EXPECT_EQ(result.error().code, ErrorCode::InvalidRequest);
            }
            EXPECT_TRUE(server.requests().empty());
        }
    }
}

TEST_F(WireTransport, AutomaticBusinessRequestsKeepExplicitSummaryBounds) {
    for (const auto protocol : {"messages", "openai"}) {
        WireHttpServer server({compact_reply(protocol)}, compact_reply(protocol, true));
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = protocol;
        settings.model_params.model = "small-budget-model";
        settings.model_context_limits = mixed_model_limits();
        settings.context_window.auto_compact = true;
        QueryEngine engine(std::move(settings), registry);
        isolate_compaction_system(engine);
        seed_compactable_history(engine, 600000);
        const auto result = engine.query("Finish without tools.");
        ASSERT_TRUE(result) << result.error().format();
        std::size_t summaries = 0;
        for (const auto& request : server.requests()) {
            const auto body = loom::utils::json::parse(request.body);
            ASSERT_TRUE(body);
            if (request.body.find("CONTEXT CHECKPOINT") != std::string::npos) {
                ++summaries;
                EXPECT_GT(body->root().get("max_tokens").as_int(), 0);
                EXPECT_LE(body->root().get("max_tokens").as_int(), 4096);
            } else if (std::string_view(protocol) == "openai") EXPECT_FALSE(body->root().get("max_tokens").valid());
            else EXPECT_EQ(body->root().get("max_tokens").as_int(), 4096);
        }
        EXPECT_GT(summaries, 0u);
    }
}

TEST_F(WireTransport, ModelMetadataLoadsBeforeBusinessWithWireAuthAndTenantPaths) {
    for (const auto protocol : {"messages", "openai"}) for (const bool token : {false, true})
        for (const auto suffix : {"/tenant/acme", "/tenant/acme/v1/"}) {
            SCOPED_TRACE(std::format("{} token={} {}", protocol, token, suffix));
            WireHttpServer server({compact_reply(protocol)}, {}, [](const auto&, auto& response) {
                response.set_content(R"({"data":[{"id":"transport-model","context_length":1048576,"max_output_tokens":32768}]})", "application/json");
            });
            ASSERT_TRUE(server.valid());
            ToolRegistry registry;
            auto settings = config(server, registry);
            settings.base_url = server.base_url() + suffix;
            settings.wire_api = protocol;
            if (!token) settings.auth_token.clear();
            QueryEngine engine(settings, registry);
            EXPECT_EQ(engine.max_context_tokens(), 1048576u);
            EXPECT_EQ(engine.model_context().source, "metadata");
            EXPECT_EQ(engine.model_context().output_cap_source, "metadata");
            ASSERT_TRUE(engine.query("Finish without tools."));
            const auto metadata = server.metadata_requests();
            ASSERT_EQ(metadata.size(), 1u);
            EXPECT_EQ(metadata.front().path, "/tenant/acme/v1/models");
            if (token || std::string_view(protocol) == "openai") {
                EXPECT_EQ(metadata.front().authorization, token ? "Bearer transport-bearer" : "Bearer transport-api-key");
                EXPECT_TRUE(metadata.front().api_key.empty());
            } else {
                EXPECT_TRUE(metadata.front().authorization.empty());
                EXPECT_EQ(metadata.front().api_key, "transport-api-key");
            }
            EXPECT_EQ(metadata.front().anthropic_version, std::string_view(protocol) == "messages" ? "2023-06-01" : "");
            EXPECT_EQ(server.requests().size(), 1u);
            EXPECT_EQ(server.methods(), (std::vector<std::string>{"GET", "POST"}));
        }
}

TEST_F(WireTransport, PreparedMetadataReusesOneSnapshotForEngineSwitchAndNativePolicy) {
    WireHttpServer server({compact_reply("messages")}, {}, [](const auto&, auto& response) {
        response.set_content(R"({"data":[{"id":"transport-model","context_length":1048576},{"id":"metadata-child","context_length":256000,"max_output_tokens":8192}]})", "application/json");
    });
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    auto settings = config(server, registry);
    settings.wire_api = "messages";
    prepare_model_metadata(settings);
    prepare_model_metadata(settings);
    ASSERT_TRUE(settings.model_metadata);
    EXPECT_EQ(server.metadata_requests().size(), 1u);
    const auto native = make_api_execution_context(settings);
    ASSERT_TRUE(native->model_context_config);
    ASSERT_EQ(native->model_context_config->metadata_limits.size(), 2u);
    const auto child = loom::services::compact::resolve_model_context(*native->model_context_config,
        "metadata-child", native->base_url, native->wire_api, 0);
    EXPECT_EQ(child.window_tokens, 256000u);
    EXPECT_EQ(child.source, "metadata");
    EXPECT_EQ(child.output_cap, std::optional<std::uint32_t>{8192});
    QueryEngine engine(settings, registry);
    EXPECT_EQ(engine.max_context_tokens(), 1048576u);
    auto params = engine.model_params();
    params.model = "metadata-child";
    engine.set_model_params(params);
    EXPECT_EQ(engine.max_context_tokens(), 256000u);
    EXPECT_EQ(engine.model_context().output_cap_source, "metadata");
    ASSERT_TRUE(engine.query("Finish without tools."));
    params.model = "transport-model";
    engine.set_model_params(params);
    EXPECT_EQ(engine.max_context_tokens(), 1048576u);
    EXPECT_EQ(server.metadata_requests().size(), 1u);
    EXPECT_EQ(server.requests().size(), 1u);
}

TEST_F(WireTransport, CopiedPreparedMetadataRefetchesForChangedCredentialsWireAndEndpoint) {
    WireHttpServer server({compact_reply("openai")}, {}, [](const auto& request, auto& response) {
        const auto capacity = request.get_header_value("Authorization") == "Bearer replacement-token" ? 256000 : 1048576;
        response.set_content(std::format(R"({{"data":[{{"id":"transport-model","context_length":{}}}]}})", capacity), "application/json");
    });
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    auto original = config(server, registry);
    prepare_model_metadata(original);
    ASSERT_TRUE(original.model_metadata);
    const auto original_snapshot = original.model_metadata;
    auto copied = original;
    prepare_model_metadata(copied);
    EXPECT_EQ(copied.model_metadata, original_snapshot);
    EXPECT_EQ(server.metadata_requests().size(), 1u);
    for (int change = 0; change < 4; ++change) {
        auto changed = original;
        if (change == 0) changed.auth_token = "replacement-token";
        if (change == 1) changed.api_key = "replacement-key";
        if (change == 2) changed.wire_api = "messages";
        if (change == 3) changed.base_url = server.base_url() + "/other-tenant";
        prepare_model_metadata(changed);
        ASSERT_TRUE(changed.model_metadata);
        EXPECT_NE(changed.model_metadata, original_snapshot);
        QueryEngine engine(changed, registry);
        EXPECT_EQ(engine.max_context_tokens(), change == 0 ? 256000u : 1048576u);
        EXPECT_EQ(server.metadata_requests().size(), static_cast<std::size_t>(change + 2));
    }
    QueryEngine unaffected(original, registry);
    EXPECT_EQ(unaffected.max_context_tokens(), 1048576u);
    EXPECT_EQ(original.model_metadata, original_snapshot);
    EXPECT_EQ(server.metadata_requests().size(), 5u);
}

TEST_F(WireTransport, MetadataFailuresRemainAnAttemptedSnapshotAndDoNotBlockBusinessRequests) {
    for (const auto failure : {"malformed", "missing-capacity", "not-found", "redirect", "transport"}) {
        SCOPED_TRACE(failure);
        WireHttpServer server({compact_reply("openai")}, {}, [failure](const auto&, auto& response) {
            const std::string_view kind(failure);
            if (kind == "malformed") response.set_content("not-json", "application/json");
            else if (kind == "missing-capacity") response.set_content(R"({"data":[{"id":"transport-model","usage":{"input_tokens":1048576}}]})", "application/json");
            else if (kind == "not-found") response.status = 404;
            else if (kind == "redirect") response.set_redirect("/redirect-target");
            else response.set_content_provider(32, "application/json", [](std::size_t, std::size_t, httplib::DataSink&) { return false; });
        });
        ASSERT_TRUE(server.valid());
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.context_window.max_context_tokens = 234567;
        prepare_model_metadata(settings);
        ASSERT_TRUE(settings.model_metadata);
        EXPECT_TRUE(settings.model_metadata->limits.empty());
        prepare_model_metadata(settings);
        QueryEngine engine(settings, registry);
        EXPECT_EQ(engine.max_context_tokens(), 234567u);
        EXPECT_EQ(engine.model_context().source, "fallback");
        ASSERT_TRUE(engine.query("Finish without tools."));
        EXPECT_EQ(server.metadata_requests().size(), 1u);
        EXPECT_EQ(server.requests().size(), 1u);
        EXPECT_EQ(server.methods(), (std::vector<std::string>{"GET", "POST"}));
    }
}

TEST_F(WireTransport, MetadataReceiverRejectsOversizedBodyBeforeUsingAdvertisedCapacity) {
    WireHttpServer server({compact_reply("messages")}, {}, [](const auto&, auto& response) {
        const auto body = std::string(R"({"data":[{"id":"transport-model","context_length":1048576}],"padding":")") +
            std::string(4 * 1024 * 1024, 'x') + "\"}";
        response.set_content(body, "application/json");
    });
    ASSERT_TRUE(server.valid());
    ToolRegistry registry;
    auto settings = config(server, registry);
    settings.wire_api = "messages";
    QueryEngine engine(settings, registry);
    EXPECT_EQ(engine.model_context().source, "fallback");
    EXPECT_EQ(engine.max_context_tokens(), 200000u);
    ASSERT_TRUE(engine.query("Finish without tools."));
    EXPECT_EQ(server.metadata_requests().size(), 1u);
    EXPECT_EQ(server.requests().size(), 1u);
}

TEST_F(WireTransport, MetadataSlowHeadersAndContinuousBodyRespectRequestDeadlineAndStillPost) {
    for (const bool slow_headers : {true, false}) {
        SCOPED_TRACE(slow_headers);
        WireHttpServer server({compact_reply("messages")}, {}, [slow_headers](const auto&, auto& response) {
            if (slow_headers) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                response.set_content(R"({"data":[{"id":"transport-model","context_length":1048576}]})", "application/json");
            } else {
                auto chunk = std::make_shared<int>(0);
                response.set_chunked_content_provider("application/json", [chunk](std::size_t, httplib::DataSink& sink) {
                    if ((*chunk)++ == 0) {
                        constexpr std::string_view prefix = R"({"data":[{"id":"transport-model","context_length":1048576}],"padding":")";
                        return sink.write(prefix.data(), prefix.size());
                    }
                    if (*chunk < 40) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        return sink.write("x", 1);
                    }
                    const bool written = sink.write("\"}", 2);
                    sink.done();
                    return written;
                });
            }
        });
        ASSERT_TRUE(server.valid());
        ToolRegistry registry;
        auto settings = config(server, registry);
        settings.wire_api = "messages";
        QueryEngine engine(settings, registry);
        // The body remains below the size limit and arrives within each I/O
        // timeout. Accepting its valid capacity would expose a missing total deadline.
        EXPECT_EQ(engine.max_context_tokens(), 200000u);
        EXPECT_EQ(engine.model_context().source, "fallback");
        ASSERT_TRUE(engine.query("Finish without tools."));
        EXPECT_EQ(server.metadata_requests().size(), 1u);
        EXPECT_EQ(server.requests().size(), 1u);
    }
}

TEST_F(WireTransport, AssemblyPreparesMetadataOnceBeforeNativeToolsAndEngineShareIt) {
    WireHttpServer server({compact_tool_reply("messages", 0, "capture model policy"), compact_reply("messages")}, {}, [](const auto&, auto& response) {
        response.set_content(R"({"data":[{"id":"assembly-metadata","context_length":1048576},{"id":"assembly-child","context_length":262144,"max_output_tokens":4096}]})", "application/json");
    });
    ASSERT_TRUE(server.valid());
    loom::core::Settings settings;
    settings.model.default_model = "assembly-metadata";
    loom::query::AssemblyOverrides overrides;
    overrides.base_url = server.base_url();
    overrides.api_key = "metadata-assembly-key";
    overrides.wire_api = "messages";
    overrides.cwd = root_.string();
    auto resolved = loom::query::resolve_engine_config(settings, overrides);
    ASSERT_TRUE(resolved);
    resolved->context_window.auto_compact = false;
    std::shared_ptr<const ApiExecutionContext> native;
    loom::query::AssemblyConfig assembly;
    assembly.engine = std::move(*resolved);
    assembly.register_extra_tools = [&](ToolRegistry& registry) {
        registry.register_tool(std::make_unique<TransportEcho>([&](const ToolInput& input) {
            native = input.api_context;
        }));
    };
    auto assembled = loom::query::assemble(assembly, {});
    ASSERT_TRUE(assembled) << assembled.error().message;
    auto& engine = assembled->engine();
    EXPECT_EQ(engine.max_context_tokens(), 1048576u);
    EXPECT_EQ(engine.model_context().source, "metadata");
    ASSERT_TRUE(engine.query("Call Echo once, then finish."));
    ASSERT_TRUE(native);
    ASSERT_TRUE(native->model_context_config);
    const auto child = loom::services::compact::resolve_model_context(*native->model_context_config,
        "assembly-child", native->base_url, native->wire_api, 0);
    EXPECT_EQ(child.window_tokens, 262144u);
    EXPECT_EQ(child.output_cap, std::optional<std::uint32_t>{4096});
    EXPECT_EQ(server.metadata_requests().size(), 1u);
    EXPECT_EQ(server.methods(), (std::vector<std::string>{"GET", "POST", "POST"}));
}
