/// Subagent protocol integration through real loopback HTTP transfers.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.orchestration.agent;
import loom.query.query_engine;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.teams.team_helpers;
import loom.tasks.support;
import loom.tools.tool;
import loom.tools.runtime_registry;
import loom.services.api.streaming;
import loom.services.api.client;
import loom.serdes.json;
import loom.orchestration.agent.utils;
import loom.services.compact.context_compaction;
import loom.config.config;
import loom.query.assembly;

namespace {
namespace api = loom::services::api;
namespace runtime = loom::tools::agent_runtime;
namespace json = loom::utils::json;

struct AgentWireEnv {
    std::string key;
    std::optional<std::string> previous;
    AgentWireEnv(std::string name, const std::string& value) : key(std::move(name)) {
        if (const auto* old = std::getenv(key.c_str())) previous = old;
        ::setenv(key.c_str(), value.c_str(), 1);
    }
    ~AgentWireEnv() {
        if (previous) ::setenv(key.c_str(), previous->c_str(), 1);
        else ::unsetenv(key.c_str());
    }
};

struct AgentWireSandbox {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("loom_agent_wire_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AgentWireEnv runtime_path{"LOOM_AGENT_RUNTIME_DIR", path.string()};
    AgentWireSandbox() { std::filesystem::create_directories(path); runtime::native_agent_store().clear_for_testing(); }
    ~AgentWireSandbox() {
        runtime::native_agent_store().wait_for_background_agents();
        runtime::native_agent_store().clear_for_testing();
        std::filesystem::remove_all(path);
    }
};

[[nodiscard]] std::string openai_frame(std::string_view value) {
    return "data: " + std::string(value) + "\n\n";
}

[[nodiscard]] std::string openai_text(std::string_view text, int tokens = 3) {
    json::JsonMutDoc doc;
    auto delta = doc.object();
    delta.add("content", doc.string(text));
    auto choice = doc.object();
    choice.add("delta", delta);
    choice.add("finish_reason", doc.string("stop"));
    auto choices = doc.array();
    choices.append(choice);
    auto root = doc.object();
    root.add("choices", choices);
    doc.set_root(root);
    return openai_frame(doc.to_string()) +
        openai_frame(std::format(R"({{"choices":[],"usage":{{"prompt_tokens":11,"completion_tokens":{}}}}})", tokens)) +
        openai_frame("[DONE]");
}

[[nodiscard]] std::string messages_text(std::string_view text) {
    json::JsonMutDoc doc;
    auto delta = doc.object();
    delta.add("type", doc.string("text_delta"));
    delta.add("text", doc.string(text));
    auto root = doc.object();
    root.add("index", doc.number(static_cast<int64_t>(0)));
    root.add("delta", delta);
    doc.set_root(root);
    return "event: message_start\ndata: {\"message\":{\"id\":\"session-probe\",\"role\":\"assistant\",\"model\":\"wire-model\",\"content\":[]}}\n\n"
        "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
        "event: content_block_delta\ndata: " + doc.to_string() + "\n\n"
        "event: content_block_stop\ndata: {\"index\":0}\n\n"
        "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":3}}\n\n"
        "event: message_stop\ndata: {}\n\n";
}

class AgentWireServer {
public:
    using Responder = std::function<void(const httplib::Request&, httplib::Response&, std::size_t)>;
    explicit AgentWireServer(Responder responder) {
        server_.new_task_queue = [] { return new httplib::ThreadPool(4); };
        server_.Post(R"(/.*)", [this, responder](const auto& request, auto& response) {
            std::size_t index;
            {
                std::lock_guard lock(mutex_);
                index = bodies_.size();
                bodies_.push_back(request.body);
                auth_.push_back(request.get_header_value("Authorization"));
                messages_auth_.push_back(request.has_header("x-api-key"));
            }
            cv_.notify_all();
            responder(request, response, index);
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~AgentWireServer() { server_.stop(); if (thread_.joinable()) thread_.join(); }
    [[nodiscard]] std::string url() const { return std::format("http://127.0.0.1:{}", port_); }
    [[nodiscard]] std::vector<std::string> bodies() const { std::lock_guard lock(mutex_); return bodies_; }
    [[nodiscard]] std::vector<std::string> auth() const { std::lock_guard lock(mutex_); return auth_; }
    [[nodiscard]] bool used_messages_auth() const {
        std::lock_guard lock(mutex_);
        return std::ranges::any_of(messages_auth_, [](bool value) { return value; });
    }
    [[nodiscard]] bool wait_for_requests(std::size_t count) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(3), [&] { return bodies_.size() >= count; });
    }
private:
    httplib::Server server_;
    int port_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> bodies_;
    std::vector<std::string> auth_;
    std::vector<bool> messages_auth_;
};

[[nodiscard]] std::shared_ptr<const loom::core::ApiExecutionContext> agent_wire_context(
    const AgentWireServer& server, std::string token = "session-token") {
    auto context = std::make_shared<loom::core::ApiExecutionContext>();
    context->wire_api = "openai";
    context->base_url = server.url();
    context->api_key = "session-key";
    context->auth_token = std::move(token);
    return context;
}

[[nodiscard]] loom::core::ToolInput agent_wire_input(std::string_view id, bool background = false) {
    return loom::core::ToolInput::from_json(std::format(
        R"({{"prompt":"Complete wire probe","agent_id":"{}","run_in_background":{}}})", id, background));
}

[[nodiscard]] std::string result_text(const loom::core::ToolResult& result) {
    std::string text;
    for (const auto& content : result.content) text += content.text;
    return text;
}

class WireProbeTool final : public loom::core::ITool {
    loom::core::ToolDefinition definition_{
        .name = "WireProbe", .description = "Record a protocol probe",
        .input_schema = {.properties = {{.name = "value", .type = "string", .required = true}}},
        .permission = loom::core::ToolPermission::ReadOnly};
public:
    std::vector<std::string> inputs;
    [[nodiscard]] const loom::core::ToolDefinition& definition() const override { return definition_; }
    [[nodiscard]] bool check_permission(const loom::core::ToolInput&) const override { return true; }
    [[nodiscard]] loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput& input) override {
        inputs.emplace_back(input.json());
        return loom::core::ToolResult::success("recorded");
    }
};

class CancellableWireProbeTool final : public loom::core::ITool {
    loom::core::ToolDefinition definition_{
        .name = "CancellableProbe", .description = "Pause after a possible side effect",
        .permission = loom::core::ToolPermission::Write};
    std::filesystem::path marker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_ = false;
    bool released_ = false;
public:
    std::atomic<int> calls{0};
    explicit CancellableWireProbeTool(std::filesystem::path marker) : marker_(std::move(marker)) {}
    [[nodiscard]] const loom::core::ToolDefinition& definition() const override { return definition_; }
    [[nodiscard]] bool check_permission(const loom::core::ToolInput&) const override { return true; }
    [[nodiscard]] loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
        ++calls;
        { std::ofstream marker(marker_); marker << "effect occurred before cancellation\n"; }
        std::unique_lock lock(mutex_);
        entered_ = true;
        cv_.notify_all();
        if (!cv_.wait_for(lock, std::chrono::seconds(5), [&] { return released_; })) {
            return loom::core::ToolResult::error("Cancellation fixture was not released");
        }
        return loom::core::ToolResult::error("Tool cancelled after the marker was written");
    }
    [[nodiscard]] bool wait_until_entered() {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(3), [&] { return entered_; });
    }
    void release() {
        { std::lock_guard lock(mutex_); released_ = true; }
        cv_.notify_all();
    }
};
} // namespace

TEST(AgentWire, SseFieldsSurvivePartialFrameFeeds) {
    api::StreamParser parser;
    parser.start();
    parser.feed("event: message_start\n");
    auto pending = parser.next_event();
    ASSERT_TRUE(pending);
    EXPECT_FALSE(pending->has_value());
    parser.feed("data: {\"message\":{\"id\":\"partial\",\"usage\":{\"input_tokens\":11}}}\n\n");
    auto event = parser.next_event();
    ASSERT_TRUE(event && event->has_value());
    EXPECT_EQ((**event).type, api::StreamEventType::MessageStart);
    EXPECT_EQ((**event).input_tokens, 11);
}

TEST(AgentWire, ExplicitSessionContextWinsAndUsesRealUsage) {
    AgentWireSandbox sandbox;
    AgentWireEnv wrong_protocol("LOOM_WIRE_API", "messages");
    AgentWireEnv wrong_url("LOOM_BASE_URL", "http://127.0.0.1:1");
    AgentWireEnv wrong_key("LOOM_API_KEY", "environment-key");
    AgentWireServer server([](const auto& request, auto& response, std::size_t) {
        EXPECT_EQ(request.path, "/v1/chat/completions");
        response.set_content(openai_text("session wire complete"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    auto result = tool.execute(agent_wire_input("wire-context"));
    ASSERT_TRUE(result) << result.error().format();
    ASSERT_FALSE(result->is_error) << result_text(*result);
    EXPECT_EQ(result_text(*result), "session wire complete");
    EXPECT_EQ(tool.last_run_turns(), 1);
    EXPECT_EQ(tool.last_run_tokens(), 14);
    EXPECT_EQ(server.auth(), (std::vector<std::string>{"Bearer session-token"}));
    EXPECT_FALSE(server.used_messages_auth());
    auto body = json::parse(server.bodies().at(0));
    ASSERT_TRUE(body);
    EXPECT_EQ(body->root().get("model").as_str(), "wire-model");
    EXPECT_TRUE(body->root().get("stream_options").get("include_usage").as_bool());
    for (const auto& artifact : std::filesystem::recursive_directory_iterator(sandbox.path)) {
        if (!artifact.is_regular_file()) continue;
        std::ifstream input(artifact.path());
        const std::string persisted((std::istreambuf_iterator<char>(input)), {});
        EXPECT_EQ(persisted.find("session-key"), std::string::npos) << artifact.path();
        EXPECT_EQ(persisted.find("session-token"), std::string::npos) << artifact.path();
    }
}

TEST(AgentWire, IndexedToolArgumentsStaySeparateAcrossInterleaving) {
    AgentWireSandbox sandbox;
    AgentWireServer server([](const auto&, auto& response, std::size_t index) {
        if (index) { response.set_content(openai_text("both probes complete"), "text/event-stream"); return; }
        response.set_content(
            openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call_a","function":{"name":"WireProbe","arguments":"{\"value\":\""}},{"index":1,"id":"call_b","function":{"name":"WireProbe","arguments":"{\"value\":\""}}]},"finish_reason":null}]})") +
            openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":1,"function":{"arguments":"second\"}"}},{"index":0,"function":{"arguments":"first\"}"}}]},"finish_reason":null}]})") +
            openai_frame(R"({"choices":[{"delta":{},"finish_reason":"tool_calls"}],"usage":{"prompt_tokens":11,"completion_tokens":7}})") + openai_frame("[DONE]"),
            "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    auto probe = std::make_unique<WireProbeTool>();
    auto* probe_ptr = probe.get();
    registry.register_tool(std::move(probe));
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
    loom::tools::AgentTool tool(config, 0, &registry, allow);
    auto result = tool.execute(agent_wire_input("wire-indexed"));
    ASSERT_TRUE(result) << result.error().format();
    ASSERT_FALSE(result->is_error) << result_text(*result);
    EXPECT_EQ(probe_ptr->inputs, (std::vector<std::string>{R"({"value":"first"})", R"({"value":"second"})"}));
    EXPECT_EQ(tool.last_run_turns(), 2);
    EXPECT_EQ(tool.last_run_tokens(), 32);
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 2u);
    auto second = json::parse(bodies[1]);
    ASSERT_TRUE(second);
    auto messages = second->root().get("messages");
    std::vector<std::string> result_ids;
    messages.iter([&](json::JsonVal message) {
        if (message.get("role").as_str() == "tool") result_ids.emplace_back(message.get("tool_call_id").as_str());
    });
    EXPECT_EQ(result_ids, (std::vector<std::string>{"call_a", "call_b"}));
}

TEST(AgentWire, RejectsHttpErrorsAndTruncatedStreams) {
    for (const auto mode : {"http", "missing-stop", "invalid-json", "missing-finish"}) {
        SCOPED_TRACE(mode);
        AgentWireSandbox sandbox;
        AgentWireServer server([mode](const auto&, auto& response, std::size_t) {
            if (std::string_view(mode) == "http") {
                response.status = 401;
                response.set_content(openai_text("must not count as success"), "text/event-stream");
            } else if (std::string_view(mode) == "missing-stop") {
                response.set_content(openai_frame(R"({"choices":[{"delta":{"content":"partial"},"finish_reason":"stop"}]})"), "text/event-stream");
            } else if (std::string_view(mode) == "missing-finish") {
                response.set_content(openai_frame("[DONE]"), "text/event-stream");
            } else {
                response.set_content(openai_frame("{invalid"), "text/event-stream");
            }
        });
        loom::core::ToolRegistry registry;
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.api_context = agent_wire_context(server);
        loom::tools::AgentTool tool(config, 0, &registry);
        auto result = tool.execute(agent_wire_input("wire-error"));
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->is_error) << result_text(*result);
        const auto record = runtime::native_agent_store().get("wire-error");
        ASSERT_TRUE(record);
        EXPECT_EQ(record->status, runtime::NativeAgentStatus::Failed);
    }
}

namespace {
std::string native_checkpoint_summary() {
    return "## Goal\nFinish the current request.\n## Constraints\nDo not commit; preserve late-constraint-783."
        "\n## Decisions\nUse the completed investigation.\n## Progress\nThe completed tool result is 47."
        "\n## Next Steps\nReturn the answer.\n## References\nprobe.txt\n";
}

void seed_native_compaction_history(std::string_view id, std::size_t observation_size = 24'000) {
    auto record = runtime::make_native_agent_record(std::string(id), "general-purpose");
    record.status = runtime::NativeAgentStatus::Completed;
    runtime::native_agent_store().upsert(std::move(record));
    namespace agent = loom::tools::agent::utils;
    agent::append_agent_sidechain_message(id, api::Message::from_text("user", "Original investigation"),
        std::nullopt, true);
    agent::append_agent_sidechain_message(id, api::Message::from_text("assistant",
        "old-historical-observation " + std::string(observation_size, 'q') +
        " late-constraint-783; do not commit; completed tool result 47"));
}

std::shared_ptr<loom::core::ApiExecutionContext> native_compaction_context(
    const AgentWireServer& server, std::string_view protocol, bool proactive = true) {
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = protocol;
    context->context_window_tokens = 4000;
    context->context_output_tokens = 128;
    context->context_auto_compact = proactive;
    return context;
}

bool native_summary_request(const httplib::Request& request) {
    const auto parsed = json::parse(request.body);
    return parsed && parsed->root().get("tools").size() == 0;
}

class NativeGrowingProbe final : public loom::core::ITool {
public:
    int calls = 0;
    const loom::core::ToolDefinition& definition() const override { return definition_; }
    bool check_permission(const loom::core::ToolInput&) const override { return true; }
    loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
        ++calls;
        return loom::core::ToolResult::success("completed tool result 47 " + std::string(8'000, 'z'));
    }
private:
    loom::core::ToolDefinition definition_{.name = "NativeGrowingProbe",
        .description = "Return a large completed observation", .permission = loom::core::ToolPermission::ReadOnly};
};

class NativeArithmeticProbe final : public loom::core::ITool {
public:
    const loom::core::ToolDefinition& definition() const override { return definition_; }
    bool check_permission(const loom::core::ToolInput&) const override { return true; }
    loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
        return loom::core::ToolResult::success(std::to_string(19 + 28));
    }
private:
    loom::core::ToolDefinition definition_{.name = "CheckpointArithmetic",
        .description = "Return the verified sum of 19 and 28", .permission = loom::core::ToolPermission::ReadOnly};
};

std::string native_compaction_tool_call(std::string_view protocol, int number) {
    const auto id = "native-growing-" + std::to_string(number);
    if (protocol == "openai") return openai_frame(std::format(
        R"({{"choices":[{{"delta":{{"tool_calls":[{{"index":0,"id":"{}","type":"function","function":{{"name":"NativeGrowingProbe","arguments":"{{}}"}}}}]}} ,"finish_reason":"tool_calls"}}]}})", id)) + openai_frame("[DONE]");
    return "event: message_start\ndata: {\"message\":{\"id\":\"growing\"}}\n\n"
        "event: content_block_start\ndata: " + std::format(
        R"({{"index":0,"content_block":{{"type":"tool_use","id":"{}","name":"NativeGrowingProbe","input":{{}}}}}})", id) +
        "\n\nevent: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{}\"}}\n\n"
        "event: content_block_stop\ndata: {\"index\":0}\n\n"
        "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"tool_use\"}}\n\n"
        "event: message_stop\ndata: {}\n\n";
}

auto resume_native_compaction_input(std::string_view id) {
    return loom::core::ToolInput::from_json(std::format(
        R"({{"prompt":"Complete current request verbatim","agent_id":"{}","resume_existing":true}})", id));
}

std::shared_ptr<loom::core::ApiExecutionContext> native_model_context(
    const AgentWireServer& server, std::string_view protocol, std::uint32_t parent_window = 200000) {
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = protocol;
    context->context_window_tokens = parent_window;
    context->context_output_tokens = 16384;
    auto policy = std::make_shared<loom::core::ModelContextConfig>();
    policy->fallback_window_tokens = 200000;
    policy->limits = {
        {.model = "native-large", .context_window_tokens = 1000000, .max_output_tokens = 32768},
        {.model = "native-small", .context_window_tokens = 256000, .max_output_tokens = 1024},
    };
    context->model_context_config = std::move(policy);
    return context;
}

loom::core::ToolInput native_model_resume_input(std::string_view id, std::string_view model) {
    return loom::core::ToolInput::from_json(std::format(
        R"({{"prompt":"Continue the current model-specific task","agent_id":"{}","resume_existing":true,"model":"{}"}})",
        id, model));
}

std::string native_agent_model_call(std::string_view protocol, std::string_view child_id,
    std::string_view model, bool resume = false) {
    json::JsonMutDoc args;
    auto input = args.object();
    input.add("prompt", args.string("Complete the nested model task"));
    input.add("agent_id", args.string(child_id));
    input.add("subagent_type", args.string("general-purpose"));
    input.add("model", args.string(model));
    input.add("resume_existing", args.boolean(resume));
    args.set_root(input);
    json::JsonMutDoc doc;
    auto root = doc.object();
    if (protocol == "openai") {
        auto function = doc.object();
        function.add("name", doc.string("Agent"));
        function.add("arguments", doc.string(args.to_string()));
        auto call = doc.object();
        call.add("index", doc.number(static_cast<int64_t>(0)));
        call.add("id", doc.string("delegate-" + std::string(child_id)));
        call.add("function", function);
        auto calls = doc.array();
        calls.append(call);
        auto delta = doc.object();
        delta.add("tool_calls", calls);
        auto choice = doc.object();
        choice.add("delta", delta);
        choice.add("finish_reason", doc.string("tool_calls"));
        auto choices = doc.array();
        choices.append(choice);
        root.add("choices", choices);
        doc.set_root(root);
        return openai_frame(doc.to_string()) + openai_frame("[DONE]");
    }
    auto delta = doc.object();
    delta.add("type", doc.string("input_json_delta"));
    delta.add("partial_json", doc.string(args.to_string()));
    root.add("index", doc.number(static_cast<int64_t>(0)));
    root.add("delta", delta);
    doc.set_root(root);
    return "event: message_start\ndata: {\"message\":{\"id\":\"nested-model\"}}\n\n"
        "event: content_block_start\ndata: " + std::format(
            R"({{"index":0,"content_block":{{"type":"tool_use","id":"delegate-{}","name":"Agent","input":{{}}}}}})", child_id) +
        "\n\nevent: content_block_delta\ndata: " + doc.to_string() +
        "\n\nevent: content_block_stop\ndata: {\"index\":0}\n\n"
        "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"tool_use\"}}\n\n"
        "event: message_stop\ndata: {}\n\n";
}
} // namespace

TEST(AgentWire, AutomaticMainChildAndGrandchildResolveOwnModelWithoutFreezingOutput) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
        std::vector<std::string> models;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t index) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            const auto model = parsed->root().get("model").as_str();
            models.emplace_back(model);
            if (std::string_view(protocol) == "openai") EXPECT_FALSE(parsed->root().get("max_tokens").valid());
            else EXPECT_EQ(parsed->root().get("max_tokens").as_int(), model == "native-small" ? 1024 : 32768);
            EXPECT_FALSE(parsed->root().get("thinking").valid());
            if (index < 2) response.set_content(native_agent_model_call(protocol,
                index == 0 ? "auto-child" : "auto-grandchild", index == 0 ? "native-small" : "native-large"),
                "text/event-stream");
            else response.set_content(std::string_view(protocol) == "openai" ? openai_text("automatic chain complete") :
                messages_text("automatic chain complete"), "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
        registry.register_tool(loom::tools::make_agent_tool({}, 0, &registry, allow));
        loom::core::QueryEngineConfig config;
        config.model_params.model = "native-large";
        config.model_params.max_tokens = 0;
        config.api_key = "session-key";
        config.auth_token = "session-token";
        config.base_url = server.url();
        config.wire_api = protocol;
        config.cwd = sandbox.path.string();
        config.context_window.auto_compact = false;
        config.model_context_limits = {
            {.model = "native-large", .context_window_tokens = 1000000, .max_output_tokens = 32768},
            {.model = "native-small", .context_window_tokens = 256000, .max_output_tokens = 1024}};
        loom::core::QueryEngine engine(config, registry);
        engine.set_session_id("auto-chain-owner");
        loom::core::QueryOptions options;
        bool failed = false;
        options.on_event = [&](const auto& event) {
            if (const auto* error = std::get_if<loom::core::StreamError>(&event)) {
                ADD_FAILURE() << error->message;
                failed = true;
            }
        };
        engine.stream_query("Delegate the model-specific task", options);
        EXPECT_FALSE(failed);
        EXPECT_EQ(models, (std::vector<std::string>{"native-large", "native-small", "native-large", "native-small", "native-large"}));
        EXPECT_EQ(engine.model_params().max_tokens, 0u);
    }
}

TEST(AgentWire, AutomaticNativeTransportPreservesLegacyWindowAndValidatesManualThinkingBeforeHttp) {
    namespace agent = loom::tools::agent::utils;
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        AgentWireServer server([&](const auto&, auto& response, std::size_t) {
            response.set_content(std::string_view(protocol) == "openai" ? openai_text("automatic complete") :
                messages_text("automatic complete"), "text/event-stream");
        });
        auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
        context->wire_api = protocol;
        context->context_window_tokens = 12345;
        context->context_output_tokens = 0;
        const auto policy = agent::agent_context_policy(context, "unknown-private-model");
        EXPECT_EQ(policy.window_tokens, 12345u);
        EXPECT_EQ(policy.output_tokens, 6049u);
        api::MessagesClient client(agent::agent_api_client_config(context));
        api::CreateMessageRequest request;
        request.model = "unknown-private-model";
        request.messages = {api::Message::from_text("user", "Complete once")};
        request.max_tokens = 0;
        {
            auto stream = agent::create_agent_model_stream(client, request, context);
            ASSERT_TRUE(stream);
            while (!stream->is_finished()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto bodies = server.bodies();
        ASSERT_EQ(bodies.size(), 1u);
        const auto body = json::parse(bodies.front());
        ASSERT_TRUE(body);
        if (std::string_view(protocol) == "openai") EXPECT_FALSE(body->root().get("max_tokens").valid());
        else EXPECT_EQ(body->root().get("max_tokens").as_int(), 6049);
        if (std::string_view(protocol) == "messages") {
            request.thinking_budget = 6049;
            const auto size = agent::agent_serialized_request_size(client, request, context);
            ASSERT_FALSE(size);
            EXPECT_NE(size.error().message().find("Manual thinking budget"), std::string::npos);
            EXPECT_FALSE(agent::create_agent_model_stream(client, request, context));
            request.max_tokens = 64;
            request.thinking_budget = 64;
            EXPECT_FALSE(agent::create_agent_model_stream(client, request, context));
            EXPECT_EQ(server.bodies().size(), 1u);
        }
    }
}

TEST(AgentWire, AutomaticNativeManualThinkingRaisesOnlyAutomaticOutputWithinModelLimits) {
    namespace agent = loom::tools::agent::utils;
    AgentWireSandbox sandbox;
    AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
        const auto body = json::parse(request.body);
        ASSERT_TRUE(body);
        EXPECT_EQ(body->root().get("max_tokens").as_int(), 20001);
        EXPECT_EQ(body->root().get("thinking").get("budget_tokens").as_int(), 20000);
        response.set_content(messages_text("manual complete"), "text/event-stream");
    });
    auto context = native_model_context(server, "messages");
    context->context_output_tokens = 0;
    api::MessagesClient client(agent::agent_api_client_config(context));
    api::CreateMessageRequest request;
    request.model = "unknown-private-model";
    request.messages = {api::Message::from_text("user", "Complete once")};
    request.max_tokens = 0;
    request.thinking_budget = 20000;
    {
        auto stream = agent::create_agent_model_stream(client, request, context);
        ASSERT_TRUE(stream);
        while (!stream->is_finished()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(server.bodies().size(), 1u);
    request.model = "native-small";
    EXPECT_FALSE(agent::create_agent_model_stream(client, request, context));
    request.model = "native-large";
    request.max_tokens = 10000;
    EXPECT_FALSE(agent::create_agent_model_stream(client, request, context));
    EXPECT_EQ(server.bodies().size(), 1u);
}

TEST(AgentWire, ModelPolicyResolvesActualChildAtRealMillionAnd256KLimits) {
    for (const bool automatic : {false, true}) for (const auto protocol : {"messages", "openai"}) for (const bool parent_large : {true, false}) {
        SCOPED_TRACE(std::format("{} parent_large={}", protocol, parent_large));
        AgentWireSandbox sandbox;
        seed_native_compaction_history("model-child", 1200000);
        const std::string child_model = parent_large ? "native-small" : "native-large";
        int business = 0;
        int summaries = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            EXPECT_EQ(parsed->root().get("model").as_str(), child_model);
            if (native_summary_request(request)) {
                ++summaries;
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 1024);
            } else {
                ++business;
                if (automatic && std::string_view(protocol) == "openai")
                    EXPECT_FALSE(parsed->root().get("max_tokens").valid());
                else EXPECT_EQ(parsed->root().get("max_tokens").as_int(),
                    parent_large ? 1024 : (automatic ? 32768 : 16384));
                EXPECT_EQ(request.body.find("old-historical-observation") == std::string::npos, parent_large);
            }
            const auto text = native_summary_request(request) ? native_checkpoint_summary() : "model child complete";
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text), "text/event-stream");
        });
        auto context = native_model_context(server, protocol, parent_large ? 1000000 : 256000);
        if (automatic) context->context_output_tokens = 0;
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "stale-registered-model";
        config.api_context = context;
        loom::tools::AgentTool tool(config, 0, &registry);
        auto input = native_model_resume_input("model-child", child_model);
        input.parent_model = parent_large ? "native-large" : "native-small";
        const auto result = tool.execute(input);
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_EQ(business, 1);
        EXPECT_EQ(summaries > 0, parent_large);
        EXPECT_LE(summaries, 16);
        EXPECT_EQ(context->context_window_tokens, parent_large ? 1000000u : 256000u);
        EXPECT_EQ(context->context_output_tokens, automatic ? 0u : 16384u);
    }
}

TEST(AgentWire, ModelPolicyResumeSwitchRechecksWindowAndRestoresRequestedOutput) {
    for (const bool automatic : {false, true}) for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("model-resume-switch", 1200000);
        std::vector<std::string> business_models;
        int summaries = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            const auto model = parsed->root().get("model").as_str();
            if (native_summary_request(request)) {
                ++summaries;
                EXPECT_EQ(model, "native-small");
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 1024);
            } else {
                business_models.emplace_back(model);
                if (automatic && std::string_view(protocol) == "openai")
                    EXPECT_FALSE(parsed->root().get("max_tokens").valid());
                else EXPECT_EQ(parsed->root().get("max_tokens").as_int(),
                    model == "native-small" ? 1024 : (automatic ? 32768 : 16384));
                EXPECT_EQ(request.body.find("old-historical-observation") != std::string::npos,
                    business_models.size() == 1);
            }
            const auto text = native_summary_request(request) ? native_checkpoint_summary() : "switch complete";
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text), "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "native-large";
        auto context = native_model_context(server, protocol, 1000000);
        if (automatic) context->context_output_tokens = 0;
        config.api_context = context;
        loom::tools::AgentTool tool(config, 0, &registry);
        int after_small = 0;
        for (const auto model : {"native-large", "native-small", "native-large"}) {
            runtime::native_agent_store().clear_for_testing();
            const auto result = tool.execute(native_model_resume_input("model-resume-switch", model));
            ASSERT_TRUE(result);
            ASSERT_FALSE(result->is_error) << result_text(*result);
            if (std::string_view(model) == "native-small") { EXPECT_GT(summaries, 0); after_small = summaries; }
        }
        EXPECT_EQ(business_models, (std::vector<std::string>{"native-large", "native-small", "native-large"}));
        EXPECT_EQ(summaries, after_small);
        EXPECT_EQ(config.api_context->context_output_tokens, automatic ? 0u : 16384u);
    }
}

TEST(AgentWire, ModelPolicyGrandchildUsesOwnWindowAndOriginalRequestedOutput) {
    for (const bool automatic : {false, true}) for (const auto protocol : {"messages", "openai"}) for (const bool outer_large : {true, false}) {
        SCOPED_TRACE(std::format("{} outer_large={}", protocol, outer_large));
        AgentWireSandbox sandbox;
        AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
        seed_native_compaction_history("policy-leaf", 1200000);
        const std::string outer_model = outer_large ? "native-large" : "native-small";
        const std::string middle_model = outer_large ? "native-small" : "native-large";
        std::vector<std::string> models;
        int summaries = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            const auto model = parsed->root().get("model").as_str();
            if (native_summary_request(request)) {
                ++summaries;
                EXPECT_EQ(model, outer_model);
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 1024);
                response.set_content(std::string_view(protocol) == "openai" ? openai_text(native_checkpoint_summary()) :
                    messages_text(native_checkpoint_summary()), "text/event-stream");
                return;
            }
            models.emplace_back(model);
            if (automatic && std::string_view(protocol) == "openai")
                EXPECT_FALSE(parsed->root().get("max_tokens").valid());
            else EXPECT_EQ(parsed->root().get("max_tokens").as_int(),
                model == "native-small" ? 1024 : (automatic ? 32768 : 16384));
            if (models.size() == 1 || models.size() == 2) {
                response.set_content(native_agent_model_call(protocol, models.size() == 1 ? "policy-middle" : "policy-leaf",
                    models.size() == 1 ? middle_model : outer_model, models.size() == 2), "text/event-stream");
            } else response.set_content(std::string_view(protocol) == "openai" ? openai_text("nested policy complete") :
                messages_text("nested policy complete"), "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        registry.register_tool(loom::tools::make_agent_tool({}, 0, &registry));
        loom::tools::AgentConfig config;
        config.default_model = outer_model;
        auto context = native_model_context(server, protocol, outer_large ? 256000 : 1000000);
        if (automatic) context->context_output_tokens = 0;
        config.api_context = context;
        auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
        loom::tools::AgentTool tool(config, 0, &registry, allow);
        const auto result = tool.execute(agent_wire_input("policy-outer"));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_EQ(models, (std::vector<std::string>{outer_model, middle_model, outer_model, middle_model, outer_model}));
        EXPECT_EQ(summaries > 0, !outer_large);
        EXPECT_LE(summaries, 16);
        EXPECT_EQ(config.api_context->context_output_tokens, automatic ? 0u : 16384u);
        EXPECT_EQ(server.auth(), std::vector<std::string>(server.bodies().size(), "Bearer session-token"));
    }
}

TEST(AgentWire, ModelPolicySummaryCapIsIndependentOfBusinessRequestedOutput) {
    for (const bool automatic : {false, true}) for (const auto protocol : {"messages", "openai"}) for (const bool capped : {true, false}) {
        SCOPED_TRACE(std::format("{} capped={}", protocol, capped));
        AgentWireSandbox sandbox;
        seed_native_compaction_history("summary-model-cap", 1200000);
        int summaries = 0;
        int business = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            EXPECT_EQ(parsed->root().get("model").as_str(), "native-summary");
            if (native_summary_request(request)) {
                ++summaries;
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(), capped ? 128 : 4096);
            } else {
                ++business;
                if (automatic && std::string_view(protocol) == "openai")
                    EXPECT_FALSE(parsed->root().get("max_tokens").valid());
                else EXPECT_EQ(parsed->root().get("max_tokens").as_int(), automatic ? (capped ? 128 : 16384) : 64);
            }
            const auto text = native_summary_request(request) ? native_checkpoint_summary() : "small business output";
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text), "text/event-stream");
        });
        auto context = native_model_context(server, protocol, 1000000);
        context->context_output_tokens = automatic ? 0 : 64;
        auto policy = std::make_shared<loom::core::ModelContextConfig>();
        policy->limits.push_back({.model = "native-summary", .context_window_tokens = 256000,
            .max_output_tokens = capped ? std::optional<std::uint32_t>{128} : std::nullopt});
        context->model_context_config = std::move(policy);
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "native-summary";
        config.api_context = context;
        loom::tools::AgentTool tool(config, 0, &registry);
        const auto result = tool.execute(native_model_resume_input("summary-model-cap", "native-summary"));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_GT(summaries, 0);
        EXPECT_LE(summaries, 16);
        EXPECT_EQ(business, 1);
    }
}

TEST(AgentWire, ModelPolicyParallelSessionsKeepScopedLimitsAndCredentialsSeparate) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("policy-parallel-large", 1200000);
        seed_native_compaction_history("policy-parallel-small", 1200000);
        std::mutex mutex;
        std::condition_variable cv;
        int arrived = 0;
        int first_summaries = 0;
        int second_summaries = 0;
        auto respond = [&](bool large, const auto& request, auto& response, std::size_t index) {
            if (index == 0) {
                std::unique_lock lock(mutex);
                ++arrived;
                cv.notify_all();
                EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 2; }));
            }
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            EXPECT_EQ(parsed->root().get("model").as_str(), "same-private-model");
            const bool summary = request.body.find("CONTEXT CHECKPOINT:") != std::string::npos;
            EXPECT_EQ(native_summary_request(request), summary);
            if (summary) {
                if (large) ++first_summaries;
                else ++second_summaries;
                EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 512);
            } else EXPECT_EQ(parsed->root().get("max_tokens").as_int(), large ? 16384 : 512);
            const auto text = summary ? native_checkpoint_summary() : "isolated policy complete";
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text), "text/event-stream");
        };
        AgentWireServer first([&](const auto& request, auto& response, std::size_t index) { respond(true, request, response, index); });
        AgentWireServer second([&](const auto& request, auto& response, std::size_t index) { respond(false, request, response, index); });
        auto context_for = [&](const AgentWireServer& server, bool large) {
            auto context = native_model_context(server, protocol, large ? 256000 : 1000000);
            context->auth_token = large ? "large-policy-secret" : "small-policy-secret";
            // The resolver receives the same canonical protocol used by the transport.
            if (std::string_view(protocol) == "openai") context->wire_api = "openai-compatible";
            auto policy = std::make_shared<loom::core::ModelContextConfig>();
            policy->limits.push_back({.model = "same-private-model", .context_window_tokens = large ? 1000000u : 256000u,
                .max_output_tokens = large ? 32768u : 512u, .base_url = server.url() + "/", .wire_api = protocol});
            context->model_context_config = std::move(policy);
            const auto resolved = loom::tools::agent::utils::agent_context_policy(context, "same-private-model");
            EXPECT_EQ(resolved.window_tokens, large ? 1000000u : 256000u);
            EXPECT_EQ(resolved.output_tokens, large ? 16384u : 512u);
            return context;
        };
        loom::core::ToolRegistry registry;
        loom::tools::AgentConfig config;
        config.default_model = "same-private-model";
        registry.register_tool(loom::tools::make_agent_tool(config, 0, &registry));
        // Agent itself is filtered from child requests unless nested agents are enabled.
        registry.register_tool(std::make_unique<WireProbeTool>());
        auto first_input = native_model_resume_input("policy-parallel-large", "same-private-model");
        first_input.api_context = context_for(first, true);
        auto second_input = native_model_resume_input("policy-parallel-small", "same-private-model");
        second_input.api_context = context_for(second, false);
        auto first_run = std::async(std::launch::async, [&] { return registry.execute("Agent", first_input); });
        auto second_run = std::async(std::launch::async, [&] { return registry.execute("Agent", second_input); });
        const auto first_result = first_run.get();
        const auto second_result = second_run.get();
        ASSERT_TRUE(first_result);
        ASSERT_FALSE(first_result->is_error) << result_text(*first_result);
        ASSERT_TRUE(second_result);
        ASSERT_FALSE(second_result->is_error) << result_text(*second_result);
        EXPECT_EQ(first_summaries, 0);
        EXPECT_GT(second_summaries, 0);
        EXPECT_LE(second_summaries, 16);
        EXPECT_EQ(first.bodies().size(), 1u);
        EXPECT_EQ(second.bodies().size(), static_cast<std::size_t>(second_summaries + 1));
        EXPECT_EQ(first.auth(), std::vector<std::string>(first.bodies().size(), "Bearer large-policy-secret"));
        EXPECT_EQ(second.auth(), std::vector<std::string>(second.bodies().size(), "Bearer small-policy-secret"));
    }
}

TEST(AgentWire, ModelPolicyUnknownChildUsesOriginalFallbackInsteadOfParentWindow) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("unknown-model-child", 1200000);
        int summaries = 0;
        int business = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const auto parsed = json::parse(request.body);
            ASSERT_TRUE(parsed);
            EXPECT_EQ(parsed->root().get("model").as_str(), "unknown-private-model");
            if (native_summary_request(request)) { ++summaries; EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 4096); }
            else { ++business; EXPECT_EQ(parsed->root().get("max_tokens").as_int(), 16384); }
            const auto text = native_summary_request(request) ? native_checkpoint_summary() : "fallback complete";
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text), "text/event-stream");
        });
        auto context = native_model_context(server, protocol, 1000000);
        const auto policy = loom::tools::agent::utils::agent_context_policy(context, "unknown-private-model");
        EXPECT_EQ(policy.window_tokens, 200000u);
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "native-large";
        config.api_context = context;
        loom::tools::AgentTool tool(config, 0, &registry);
        auto input = native_model_resume_input("unknown-model-child", "unknown-private-model");
        input.parent_model = "native-large";
        const auto result = tool.execute(input);
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_GT(summaries, 0);
        EXPECT_LE(summaries, 16);
        EXPECT_EQ(business, 1);
    }
}

TEST(AgentWire, ModelPolicyLegacyScalarAndNullKeepTheirExistingBudgets) {
    namespace agent = loom::tools::agent::utils;
    const auto defaults = agent::agent_context_policy({}, "gpt-4.1");
    EXPECT_EQ(defaults.window_tokens, 200000u);
    EXPECT_EQ(defaults.output_tokens, 16384u);
    auto legacy = std::make_shared<loom::core::ApiExecutionContext>();
    legacy->base_url = "https://api.openai.com/v1";
    legacy->wire_api = "openai";
    legacy->context_window_tokens = 12345;
    legacy->context_output_tokens = 3456;
    const auto policy = agent::agent_context_policy(legacy, "gpt-4.1");
    EXPECT_EQ(policy.window_tokens, 12345u);
    EXPECT_EQ(policy.output_tokens, 3456u);
    EXPECT_FALSE(legacy->model_context_config);
}

TEST(AgentWire, ContextBudgetCompactsAndResumeKeepsOnlyCommittedWorkingHistory) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("native-checkpoint");
        std::atomic<int> summaries{0};
        std::atomic<bool> summarized_late_constraint{false};
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            const bool summary = native_summary_request(request);
            if (summary) {
                ++summaries;
                if (request.body.find("late-constraint-783") != std::string::npos)
                    summarized_late_constraint = true;
                const auto parsed = json::parse(request.body);
                ASSERT_TRUE(parsed);
                EXPECT_FALSE(parsed->root().get("thinking").valid());
                EXPECT_FALSE(parsed->root().get("output_effort").valid());
                EXPECT_FALSE(parsed->root().get("task_budget").valid());
                EXPECT_FALSE(parsed->root().get("tool_choice").valid());
            } else {
                EXPECT_EQ(request.body.find("old-historical-observation"), std::string::npos);
                EXPECT_NE(request.body.find("Complete current request verbatim"), std::string::npos);
                EXPECT_NE(request.body.find("late-constraint-783"), std::string::npos);
                EXPECT_NE(request.body.find("47"), std::string::npos);
            }
            const auto text = summary ? native_checkpoint_summary() : std::string("native continuation complete");
            response.set_content(std::string_view(protocol) == "openai" ? openai_text(text) : messages_text(text),
                "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.max_turns = 1;
        config.api_context = native_compaction_context(server, protocol);
        loom::tools::AgentTool tool(config, 0, &registry);
        auto result = tool.execute(resume_native_compaction_input("native-checkpoint"));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_EQ(tool.last_run_turns(), 1);
        const auto summary_count = summaries.load();
        EXPECT_GT(summary_count, 0);
        EXPECT_TRUE(summarized_late_constraint.load());
        const auto record = runtime::native_agent_store().get("native-checkpoint");
        ASSERT_TRUE(record);
        EXPECT_TRUE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
            return entry.find("context-checkpoint") != std::string::npos;
        }));
        EXPECT_TRUE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
            return entry.find("old-historical-observation") != std::string::npos;
        }));
        const auto persisted = runtime::load_native_agent_record("native-checkpoint");
        ASSERT_TRUE(persisted);
        runtime::native_agent_store().clear_for_testing();
        auto resumed = tool.execute(resume_native_compaction_input("native-checkpoint"));
        ASSERT_TRUE(resumed);
        ASSERT_FALSE(resumed->is_error) << result_text(*resumed);
        EXPECT_EQ(summaries.load(), summary_count);
        EXPECT_GT(tool.last_run_tokens(), 0);
    }
}

TEST(AgentWire, SingleTaskToolRoundsCompactAndContinueWithOriginalRequest) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        int business = 0;
        int summaries = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            if (native_summary_request(request)) {
                ++summaries;
                response.set_content(std::string_view(protocol) == "openai"
                    ? openai_text(native_checkpoint_summary()) : messages_text(native_checkpoint_summary()),
                    "text/event-stream");
            } else if (++business <= 2) {
                response.set_content(native_compaction_tool_call(protocol, business), "text/event-stream");
            } else {
                EXPECT_NE(request.body.find("Keep this original request verbatim"), std::string::npos);
                EXPECT_NE(request.body.find("47"), std::string::npos);
                response.set_content(std::string_view(protocol) == "openai"
                    ? openai_text("all rounds complete") : messages_text("all rounds complete"), "text/event-stream");
            }
        });
        loom::core::ToolRegistry registry;
        auto probe = std::make_unique<NativeGrowingProbe>();
        auto* probe_ptr = probe.get();
        registry.register_tool(std::move(probe));
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.max_turns = 3;
        config.api_context = native_compaction_context(server, protocol);
        loom::tools::AgentTool tool(config, 0, &registry);
        auto result = tool.execute(loom::core::ToolInput::from_json(
            R"({"agent_id":"native-growing","prompt":"Keep this original request verbatim; finish the tool rounds without committing."})"));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_GT(summaries, 0);
        EXPECT_EQ(business, 3);
        EXPECT_EQ(probe_ptr->calls, 2);
        EXPECT_EQ(tool.last_run_turns(), 3);
    }
}

TEST(AgentWire, ContextOverflowRecoversOnceWithoutConsumingExtraBusinessTurns) {
    for (const auto protocol : {"messages", "openai"}) {
        for (const auto mode : {"http400", "http413", "in-band", "second-overflow"}) {
            SCOPED_TRACE(std::format("{} {}", protocol, mode));
            AgentWireSandbox sandbox;
            seed_native_compaction_history("native-reactive");
            int business = 0;
            int summaries = 0;
            AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
                if (native_summary_request(request)) {
                    ++summaries;
                    response.set_content(std::string_view(protocol) == "openai"
                        ? openai_text(native_checkpoint_summary()) : messages_text(native_checkpoint_summary()),
                        "text/event-stream");
                    return;
                }
                ++business;
                if (business == 1 || std::string_view(mode) == "second-overflow") {
                    const std::string error = R"({"error":{"type":"invalid_request_error","code":"context_length_exceeded","message":"prompt is too long"}})";
                    if (std::string_view(mode) == "in-band") {
                        response.set_content(std::string_view(protocol) == "openai"
                            ? openai_frame(error) : "event: error\ndata: " + error + "\n\n", "text/event-stream");
                    } else {
                        response.status = std::string_view(mode) == "http413" ? 413 : 400;
                        response.set_content(error, "application/json");
                    }
                    return;
                }
                EXPECT_EQ(request.body.find("old-historical-observation"), std::string::npos);
                response.set_content(std::string_view(protocol) == "openai"
                    ? openai_text("recovered") : messages_text("recovered"), "text/event-stream");
            });
            loom::core::ToolRegistry registry;
            auto probe = std::make_unique<WireProbeTool>();
            auto* probe_ptr = probe.get();
            registry.register_tool(std::move(probe));
            loom::tools::AgentConfig config;
            config.default_model = "wire-model";
            config.max_turns = 1;
            config.api_context = native_compaction_context(server, protocol, false);
            loom::tools::AgentTool tool(config, 0, &registry);
            auto result = tool.execute(resume_native_compaction_input("native-reactive"));
            ASSERT_TRUE(result);
            EXPECT_EQ(result->is_error, std::string_view(mode) == "second-overflow") << result_text(*result);
            EXPECT_EQ(business, 2);
            EXPECT_GT(summaries, 0);
            EXPECT_LE(summaries, 16);
            EXPECT_EQ(tool.last_run_turns(), 1);
            EXPECT_TRUE(probe_ptr->inputs.empty());
        }
    }
}

TEST(AgentWire, PayloadRecoveryRejectsTokenSmallerButByteLargerCheckpointBeforePublication) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("native-payload-no-progress", 3000);
        int business = 0;
        int summaries = 0;
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            if (native_summary_request(request)) {
                ++summaries;
                // JSON doubles each slash, so a token-smaller summary grows the actual body.
                const auto summary = native_checkpoint_summary() + std::string(2000, '\\');
                response.set_content(std::string_view(protocol) == "openai"
                    ? openai_text(summary) : messages_text(summary), "text/event-stream");
            } else {
                ++business;
                response.status = 413;
                response.set_content(R"({"error":{"type":"request_too_large","message":"Request too large"}})",
                    "application/json");
            }
        });
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.api_context = native_compaction_context(server, protocol, false);
        loom::tools::AgentTool tool(config, 0, &registry);
        auto result = tool.execute(resume_native_compaction_input("native-payload-no-progress"));
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->is_error);
        EXPECT_NE(result_text(*result).find("serialized request payload"), std::string::npos) << result_text(*result);
        EXPECT_EQ(business, 1);
        EXPECT_EQ(summaries, 1);
        const auto record = runtime::native_agent_store().get("native-payload-no-progress");
        ASSERT_TRUE(record);
        EXPECT_FALSE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
            return entry.find("context-checkpoint") != std::string::npos;
        }));
    }
}

TEST(AgentWire, MessagesCachedInputCountsTowardNativeUsage) {
    AgentWireSandbox sandbox;
    AgentWireServer server([](const auto&, auto& response, std::size_t) {
        auto stream = messages_text("cached usage complete");
        const auto usage = R"("usage":{"input_tokens":11,"cache_creation_input_tokens":13,"cache_read_input_tokens":17},)";
        stream.insert(stream.find("\"id\":\"session-probe\""), usage);
        response.set_content(stream, "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = native_compaction_context(server, "messages");
    loom::tools::AgentTool tool(config, 0, &registry);
    const auto result = tool.execute(agent_wire_input("native-cache-usage"));
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    EXPECT_EQ(tool.last_run_tokens(), 44); // 11 input + 13 write + 17 read + 3 output.
}

TEST(AgentWire, CheckpointWriteFailureAndConflictingHistoryPreserveAuthoritativeRecord) {
    AgentWireSandbox sandbox;
    seed_native_compaction_history("checked-write");
    auto& store = runtime::native_agent_store();
    const auto initial = store.get("checked-write");
    ASSERT_TRUE(initial);
    const auto record_path = runtime::agent_record_path("checked-write");
    std::ifstream input(record_path);
    const std::string before((std::istreambuf_iterator<char>(input)), {});
    EXPECT_FALSE(store.append_checkpoint_checked("checked-write", initial->sidechain_entries.size() + 1, "probe"));
    const auto saved = record_path.string() + ".preserved";
    std::filesystem::rename(record_path, saved);
    std::filesystem::create_directory(record_path);
    EXPECT_FALSE(store.append_checkpoint_checked("checked-write", initial->sidechain_entries.size(), "probe"));
    std::filesystem::remove(record_path);
    std::filesystem::rename(saved, record_path);
    const auto after = store.get("checked-write");
    ASSERT_TRUE(after);
    EXPECT_EQ(after->sidechain_entries, initial->sidechain_entries);
    std::ifstream restored(record_path);
    EXPECT_EQ(std::string((std::istreambuf_iterator<char>(restored)), {}), before);
}

TEST(AgentWire, MalformedCheckpointFallsBackAndRetainedThinkingSignaturesRemainExact) {
    AgentWireSandbox sandbox;
    seed_native_compaction_history("native-signed-tail");
    namespace agent = loom::tools::agent::utils;
    agent::append_agent_sidechain_message("native-signed-tail",
        api::Message::from_text("user", "Preserve the completed signed tool progress"), std::nullopt, true);
    api::Message tail;
    tail.role = "assistant";
    tail.content.push_back(api::ContentBlock{.type = api::ContentBlockType::Thinking,
        .thinking = "retained signed reasoning", .signature = "exact-original-signature"});
    tail.content.push_back(api::ContentBlock{.type = api::ContentBlockType::RedactedThinking,
        .thinking = "exact-original-redacted-data"});
    tail.content.push_back(api::ContentBlock{.type = api::ContentBlockType::Text, .text = "retained progress"});
    agent::append_agent_sidechain_message("native-signed-tail", tail);
    AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
        if (native_summary_request(request)) response.set_content(messages_text(native_checkpoint_summary()), "text/event-stream");
        else {
            EXPECT_NE(request.body.find("exact-original-signature"), std::string::npos);
            EXPECT_NE(request.body.find("exact-original-redacted-data"), std::string::npos);
            response.set_content(messages_text("signed continuation"), "text/event-stream");
        }
    });
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<WireProbeTool>());
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = native_compaction_context(server, "messages");
    const auto before = runtime::native_agent_store().get("native-signed-tail");
    ASSERT_TRUE(before);
    auto replay = agent::replay_agent_context(before->sidechain_entries);
    auto messages = replay.messages;
    auto anchor = replay.request_anchor;
    api::MessagesClient client(agent::agent_api_client_config(config.api_context));
    api::CreateMessageRequest input;
    input.model = "wire-model";
    input.messages = messages;
    loom::services::compact::UsageObservation observed;
    const auto compacted = agent::compact_agent_context(client, input, config.api_context, "native-signed-tail",
        messages, anchor, observed, "signed-tail-test", [](int, int) {}, [] { return false; });
    ASSERT_TRUE(compacted) << compacted.error().format();
    const auto original_tail = agent::message_json_object(tail);
    ASSERT_TRUE(std::ranges::any_of(messages, [&](const auto& message) {
        return agent::message_json_object(message) == original_tail;
    }));
    const auto checkpoint_record = runtime::native_agent_store().get("native-signed-tail");
    ASSERT_TRUE(checkpoint_record);
    const auto restored_messages = agent::resume_messages_from_sidechain_entries(checkpoint_record->sidechain_entries);
    ASSERT_TRUE(std::ranges::any_of(restored_messages, [&](const auto& message) {
        return agent::message_json_object(message) == original_tail;
    }));
    loom::tools::AgentTool tool(config, 0, &registry);
    auto result = tool.execute(resume_native_compaction_input("native-signed-tail"));
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    auto record = runtime::native_agent_store().get("native-signed-tail");
    ASSERT_TRUE(record);
    auto entries = record->sidechain_entries;
    const auto expected = agent::replay_agent_context(entries);
    ASSERT_TRUE(expected.checkpoint);
    entries.push_back(R"({"kind":"context-checkpoint","version":1,"requestAnchor":99,"snapshot":[{"role":"user","content":[{"type":"text","text":"invalid"}]}]})");
    entries.push_back(R"({"kind":"context-checkpoint","version":1,"snapshot":)");
    const auto actual = agent::replay_agent_context(entries);
    EXPECT_TRUE(actual.checkpoint);
    ASSERT_EQ(actual.messages.size(), expected.messages.size());
    for (std::size_t index = 0; index < actual.messages.size(); ++index)
        EXPECT_EQ(agent::message_json_object(actual.messages[index]), agent::message_json_object(expected.messages[index]));
}

TEST(AgentWire, NativeCheckpointReplaysMixedTextImageAndDocumentWithoutLoss) {
    AgentWireSandbox sandbox;
    namespace agent = loom::tools::agent::utils;
    const auto summary = api::Message::from_text("user", native_checkpoint_summary());
    api::Message mixed;
    mixed.role = "user";
    mixed.content.push_back(api::ContentBlock{.type = api::ContentBlockType::Text, .text = "keep media exactly"});
    mixed.content.push_back(api::ContentBlock{.type = api::ContentBlockType::Image,
        .media_type = "image/png", .image_data = "image-base64-exact"});
    mixed.content.push_back(api::ContentBlock{.type = api::ContentBlockType::Document,
        .media_type = "application/pdf", .image_data = "document-base64-exact"});
    auto record = runtime::make_native_agent_record("media-checkpoint", "general-purpose");
    runtime::native_agent_store().upsert(std::move(record));
    const auto checkpoint = std::format(
        R"({{"kind":"context-checkpoint","version":1,"requestAnchor":1,"snapshot":[{},{}]}})",
        agent::message_json_object(summary), agent::message_json_object(mixed));
    ASSERT_TRUE(runtime::native_agent_store().append_checkpoint_checked("media-checkpoint", 0, checkpoint));
    runtime::native_agent_store().clear_for_testing();
    const auto restored = runtime::load_native_agent_record("media-checkpoint");
    ASSERT_TRUE(restored);
    const auto replay = agent::replay_agent_context(restored->sidechain_entries);
    ASSERT_TRUE(replay.checkpoint);
    ASSERT_EQ(replay.messages.size(), 2u);
    ASSERT_EQ(replay.messages[1].content.size(), 3u);
    EXPECT_EQ(agent::message_json_object(replay.messages[1]), agent::message_json_object(mixed));
    EXPECT_EQ(replay.messages[1].content[1].type, api::ContentBlockType::Image);
    EXPECT_EQ(replay.messages[1].content[2].type, api::ContentBlockType::Document);
}

TEST(AgentWire, FailedSummaryDoesNotPublishCheckpointOrContinueBusinessRequest) {
    for (const auto protocol : {"messages", "openai"}) {
        for (const auto mode : {"empty", "truncated", "open-block", "unauthorized"}) {
            SCOPED_TRACE(std::format("{} {}", protocol, mode));
            AgentWireSandbox sandbox;
            seed_native_compaction_history("native-summary-failure");
            AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
                EXPECT_TRUE(native_summary_request(request));
                if (std::string_view(mode) == "unauthorized") {
                    response.status = 401;
                    response.set_content(R"({"error":{"type":"authentication_error","message":"not authorized"}})",
                        "application/json");
                } else if (std::string_view(mode) == "open-block") {
                    auto stream = messages_text(native_checkpoint_summary());
                    const auto start = stream.find("event: content_block_stop");
                    const auto end = stream.find("event: message_delta", start);
                    stream.erase(start, end - start);
                    // OpenAI's adapter produces balanced blocks itself; use a
                    // missing terminator to exercise its incomplete response path.
                    response.set_content(std::string_view(protocol) == "openai"
                        ? openai_frame(R"({"choices":[{"delta":{"content":"partial"},"finish_reason":"stop"}]})")
                        : stream, "text/event-stream");
                } else if (std::string_view(mode) == "truncated") {
                    response.set_content(std::string_view(protocol) == "openai"
                        ? openai_frame(R"({"choices":[{"delta":{"content":"partial"},"finish_reason":"length"}]})") + openai_frame("[DONE]")
                        : "event: message_start\ndata: {\"message\":{\"id\":\"truncated\"}}\n\n"
                          "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"max_tokens\"}}\n\n"
                          "event: message_stop\ndata: {}\n\n", "text/event-stream");
                } else {
                    response.set_content(std::string_view(protocol) == "openai" ? openai_text("") : messages_text(""),
                        "text/event-stream");
                }
            });
            loom::core::ToolRegistry registry;
            registry.register_tool(std::make_unique<WireProbeTool>());
            loom::tools::AgentConfig config;
            config.default_model = "wire-model";
            config.api_context = native_compaction_context(server, protocol);
            loom::tools::AgentTool tool(config, 0, &registry);
            auto result = tool.execute(resume_native_compaction_input("native-summary-failure"));
            ASSERT_TRUE(result);
            EXPECT_TRUE(result->is_error);
            EXPECT_EQ(server.bodies().size(), 1u);
            EXPECT_EQ(tool.last_run_turns(), 0);
            const auto record = runtime::native_agent_store().get("native-summary-failure");
            ASSERT_TRUE(record);
            EXPECT_FALSE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
                return entry.find("context-checkpoint") != std::string::npos;
            }));
        }
    }
}

TEST(AgentWire, CancellationDuringSummaryPreservesHistoryAndPublishesOneTerminalNotification) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        seed_native_compaction_history("native-summary-cancel");
        std::atomic<bool> release{false};
        AgentWireServer server([&](const auto&, auto& response, std::size_t) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!release.load() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            response.set_content(messages_text(native_checkpoint_summary()), "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.parent_session_id = "summary-cancel-session";
        config.api_context = native_compaction_context(server, protocol);
        loom::tools::AgentTool tool(config, 0, &registry);
        auto input = loom::core::ToolInput::from_json(
            R"({"prompt":"Complete current request verbatim","agent_id":"native-summary-cancel","resume_existing":true,"run_in_background":true})");
        auto result = tool.execute(input);
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_requests(1));
        runtime::native_agent_store().request_cancel("native-summary-cancel", "cancel summary probe");
        runtime::native_agent_store().wait_for_background_agents();
        release.store(true);
        const auto record = runtime::native_agent_store().get("native-summary-cancel");
        ASSERT_TRUE(record);
        EXPECT_EQ(record->status, runtime::NativeAgentStatus::Cancelled);
        EXPECT_FALSE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
            return entry.find("context-checkpoint") != std::string::npos;
        }));
        auto events = runtime::native_agent_store().take_pending_task_events("summary-cancel-session");
        EXPECT_EQ(events.size(), 1u);
        EXPECT_TRUE(runtime::native_agent_store().take_pending_task_events("summary-cancel-session").empty());
    }
}

TEST(AgentWire, SummaryInBandOverflowReducesChunkAndRetriesWithinSharedBounds) {
    AgentWireSandbox sandbox;
    seed_native_compaction_history("summary-inband-overflow");
    int summaries = 0;
    std::vector<std::size_t> sizes;
    AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
        if (!native_summary_request(request)) {
            response.set_content(openai_text("chunk recovery complete"), "text/event-stream");
            return;
        }
        sizes.push_back(request.body.size());
        if (++summaries == 1) {
            response.set_content(openai_frame(R"({"error":{"type":"invalid_request_error","code":"context_length_exceeded","message":"summary input too long"}})"),
                "text/event-stream");
        } else response.set_content(openai_text(native_checkpoint_summary()), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<WireProbeTool>());
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.max_turns = 1;
    config.api_context = native_compaction_context(server, "openai");
    loom::tools::AgentTool tool(config, 0, &registry);
    const auto result = tool.execute(resume_native_compaction_input("summary-inband-overflow"));
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    ASSERT_GE(sizes.size(), 2u);
    EXPECT_LT(sizes[1], sizes[0]);
    EXPECT_LE(summaries, 16);
    EXPECT_EQ(tool.last_run_turns(), 1);
}

TEST(AgentWire, ForkDirectiveAnchorDoesNotMoveWhenWorktreeNoticeIsAppended) {
    AgentWireSandbox sandbox;
    namespace agent = loom::tools::agent::utils;
    loom::tools::agent::AgentToolRequest request;
    request.prompt = "Keep the actual fork directive verbatim";
    request.subagent_type = "general-purpose";
    request.parent_assistant_message_entries = {R"({"role":"assistant","content":[{"type":"text","text":"parent result"}]})"};
    const auto plan = loom::tools::agent::build_agent_execution_plan(request, loom::tools::AgentConfig{});
    ASSERT_TRUE(plan) << plan.error();
    ASSERT_TRUE(plan->fork_request_anchor);
    auto messages = plan->fork_context_messages;
    const auto anchor_text = agent::message_content_text(messages[*plan->fork_request_anchor]);
    messages.push_back(api::Message::from_text("user", "Worktree notice: " + std::string(24'000, 'w')));
    auto anchor = plan->fork_request_anchor;
    auto record = runtime::make_native_agent_record("fork-anchor-checkpoint", "general-purpose");
    runtime::native_agent_store().upsert(std::move(record));
    for (std::size_t i = 0; i < messages.size(); ++i)
        agent::append_agent_sidechain_message("fork-anchor-checkpoint", messages[i], std::nullopt, anchor == i);
    AgentWireServer server([](const auto&, auto& response, std::size_t) {
        response.set_content(messages_text(native_checkpoint_summary()), "text/event-stream");
    });
    auto context = native_compaction_context(server, "messages");
    api::MessagesClient client(agent::agent_api_client_config(context));
    api::CreateMessageRequest input;
    input.model = "wire-model";
    input.messages = messages;
    loom::services::compact::UsageObservation observed;
    const auto result = agent::compact_agent_context(client, input, context, "fork-anchor-checkpoint",
        messages, anchor, observed, "fork-anchor-test", [](int, int) {}, [] { return false; });
    ASSERT_TRUE(result) << result.error().format();
    ASSERT_TRUE(anchor);
    EXPECT_EQ(agent::message_content_text(messages[*anchor]), anchor_text);
    EXPECT_NE(anchor_text.find(request.prompt), std::string::npos);
}

TEST(AgentWire, CheckpointResumeFiltersDanglingCallsAndKeepsCompletedToolProgress) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        namespace agent = loom::tools::agent::utils;
        const std::string id = "checkpoint-dangling-resume";
        auto record = runtime::make_native_agent_record(id, "general-purpose");
        record.status = runtime::NativeAgentStatus::Completed;
        auto& store = runtime::native_agent_store();
        store.upsert(std::move(record));
        api::Message completed;
        completed.role = "assistant";
        completed.content.push_back(api::ContentBlock{.type = api::ContentBlockType::ToolUse,
            .tool_use_id = "already-completed-call", .tool_name = "WireProbe", .tool_input_json = R"({"value":"completed"})"});
        api::Message observation;
        observation.role = "user";
        observation.content.push_back(api::ContentBlock{.type = api::ContentBlockType::ToolResult,
            .text = "completed result 47", .tool_use_id = "already-completed-call"});
        const auto checkpoint = std::format(
            R"({{"kind":"context-checkpoint","version":1,"requestAnchor":1,"snapshot":[{},{},{},{}]}})",
            agent::message_json_object(api::Message::from_text("user", native_checkpoint_summary())),
            agent::message_json_object(api::Message::from_text("user", "Current checkpoint task")),
            agent::message_json_object(completed), agent::message_json_object(observation));
        ASSERT_TRUE(store.append_checkpoint_checked(id, 0, checkpoint));
        api::Message dangling = completed;
        dangling.content.front().tool_use_id = "dangling-call-after-checkpoint";
        agent::append_agent_sidechain_message(id, dangling);
        store.clear_for_testing();
        AgentWireServer server([&](const auto& request, auto& response, std::size_t) {
            EXPECT_FALSE(native_summary_request(request));
            EXPECT_EQ(request.body.find("dangling-call-after-checkpoint"), std::string::npos);
            EXPECT_NE(request.body.find("already-completed-call"), std::string::npos);
            EXPECT_NE(request.body.find("completed result 47"), std::string::npos);
            response.set_content(std::string_view(protocol) == "openai" ? openai_text("continued") : messages_text("continued"),
                "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        auto probe = std::make_unique<WireProbeTool>();
        auto* executions = probe.get();
        registry.register_tool(std::move(probe));
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.api_context = native_compaction_context(server, protocol);
        loom::tools::AgentTool tool(config, 0, &registry);
        const auto result = tool.execute(resume_native_compaction_input(id));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_TRUE(executions->inputs.empty());
        EXPECT_EQ(server.bodies().size(), 1u);
    }
}

TEST(AgentWire, FreshForkHistoryCannotSuppressOrReplaceTheCurrentDirective) {
    for (const auto protocol : {"messages", "openai"}) {
        SCOPED_TRACE(protocol);
        AgentWireSandbox sandbox;
        const auto old_directive = runtime::build_fork_child_message("Old inherited request");
        json::JsonMutDoc old_doc;
        auto old = old_doc.object();
        old.add("role", old_doc.string("user"));
        old.add("content", old_doc.string(old_directive));
        old.add("requestAnchor", old_doc.boolean(true));
        old_doc.set_root(old);
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Actual new fork request must remain verbatim";
        request.subagent_type = "general-purpose";
        request.fork_context_entries = {old_doc.to_string()};
        auto plan = loom::tools::agent::build_agent_execution_plan(request, loom::tools::AgentConfig{});
        ASSERT_TRUE(plan) << plan.error();
        EXPECT_FALSE(plan->fork_context_includes_prompt);
        request.parent_assistant_message_entries = {R"({"role":"assistant","content":"parent progress"})"};
        plan = loom::tools::agent::build_agent_execution_plan(request, loom::tools::AgentConfig{});
        ASSERT_TRUE(plan) << plan.error();
        ASSERT_TRUE(plan->fork_request_anchor);
        EXPECT_NE(loom::tools::agent::utils::message_content_text(
            plan->fork_context_messages[*plan->fork_request_anchor]).find(request.prompt), std::string::npos);
        AgentWireServer server([&](const auto& input, auto& response, std::size_t) {
            EXPECT_NE(input.body.find(request.prompt), std::string::npos);
            response.set_content(std::string_view(protocol) == "openai" ? openai_text("fork continued") : messages_text("fork continued"),
                "text/event-stream");
        });
        json::JsonMutDoc doc;
        auto root = doc.object();
        root.add("prompt", doc.string(request.prompt));
        root.add("agent_id", doc.string("fresh-explicit-fork"));
        auto inherited = doc.array();
        inherited.append(doc.string(old_doc.to_string()));
        root.add("fork_context", inherited);
        doc.set_root(root);
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<WireProbeTool>());
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.api_context = native_compaction_context(server, protocol);
        loom::tools::AgentTool tool(config, 0, &registry);
        const auto result = tool.execute(loom::core::ToolInput::from_json(doc.to_string()));
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        EXPECT_EQ(server.bodies().size(), 1u);
    }
}

TEST(AgentWire, StoppedExecutionOwnerCannotPublishCheckpoint) {
    AgentWireSandbox sandbox;
    auto& store = runtime::native_agent_store();
    auto record = runtime::make_native_agent_record("stopped-checkpoint-owner", "general-purpose");
    std::stop_source owner;
    store.begin_execution(std::move(record), owner.get_token());
    owner.request_stop();
    ASSERT_TRUE(store.is_cancel_requested("stopped-checkpoint-owner"));
    EXPECT_FALSE(store.append_checkpoint_checked("stopped-checkpoint-owner", 0, "must not publish"));
    const auto after = store.get("stopped-checkpoint-owner");
    ASSERT_TRUE(after);
    EXPECT_TRUE(after->sidechain_entries.empty());
    store.end_execution("stopped-checkpoint-owner");
}

TEST(AgentWire, LaterProgressWriteFailureCannotTruncateCommittedCheckpoint) {
    AgentWireSandbox sandbox;
    auto record = runtime::make_native_agent_record("later-progress", "general-purpose");
    auto& store = runtime::native_agent_store();
    store.upsert(std::move(record));
    const auto checkpoint = std::format(
        R"({{"kind":"context-checkpoint","version":1,"requestAnchor":null,"snapshot":[{}]}})",
        loom::tools::agent::utils::message_json_object(api::Message::from_text("user", native_checkpoint_summary())));
    ASSERT_TRUE(store.append_checkpoint_checked("later-progress", 0, checkpoint));
    const auto path = runtime::agent_record_path("later-progress");
    const auto bytes = std::filesystem::file_size(path);
    const auto backup = path.string() + ".saved";
    std::filesystem::rename(path, backup);
    std::filesystem::create_symlink(backup, path);
    store.update_progress("later-progress", 0.5);
    EXPECT_EQ(std::filesystem::file_size(backup), bytes);
    const auto loaded = runtime::load_native_agent_record_from_path(backup);
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loaded->sidechain_entries.size(), 1u);
    EXPECT_NE(loaded->sidechain_entries.front().find("context-checkpoint"), std::string::npos);
    std::filesystem::remove(path);
    std::filesystem::rename(backup, path);
}

TEST(CompactionLive, NativeChildPreservesLateConstraintAndCompletedToolProgress) {
    const auto* settings_path = std::getenv("LOOM_COMPACTION_LIVE_SETTINGS");
    if (!settings_path || std::string_view(settings_path).empty())
        GTEST_SKIP() << "Set LOOM_COMPACTION_LIVE_SETTINGS to run paid native-child validation";
    const auto* evidence_path = std::getenv("LOOM_COMPACTION_LIVE_EVIDENCE");
    ASSERT_NE(evidence_path, nullptr);
    const auto evidence = std::filesystem::absolute(evidence_path) / "child";
    std::filesystem::create_directories(evidence);
    AgentWireSandbox sandbox;
    loom::core::ConfigManager manager{std::filesystem::path(settings_path)};
    ASSERT_TRUE(manager.load(loom::core::LoadOptions{.quiet = true}));
    loom::query::AssemblyOverrides overrides;
    overrides.cwd = sandbox.path.string();
    if (const auto* wire = std::getenv("LOOM_COMPACTION_LIVE_WIRE_API")) overrides.wire_api = wire;
    auto resolved = loom::query::resolve_engine_config(manager.settings(), overrides);
    ASSERT_TRUE(resolved) << resolved.error().format();
    if (const auto* token = std::getenv("LOOM_COMPACTION_LIVE_AUTH_TOKEN")) resolved->auth_token = token;
    else if (const auto* token = std::getenv("LOOM_AUTH_TOKEN")) resolved->auth_token = token;
    ASSERT_FALSE(resolved->api_key.empty() && resolved->auth_token.empty());
    auto context = std::make_shared<loom::core::ApiExecutionContext>(
        *loom::core::make_api_execution_context(*resolved));
    context->context_window_tokens = 8192;
    context->context_output_tokens = 1024;
    namespace agent = loom::tools::agent::utils;
    const auto nonce = "LATE_CHILD_CONSTRAINT_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::string background;
    for (int i = 0; i < 80; ++i) background += std::format(
        "Observation {}: use existing interfaces for reversible local verification and preserve task progress.\n", i);
    const auto constraint = "Never commit, push or publish. Repeat the exact marker " + nonce +
        " and the completed arithmetic result in the final answer.";
    auto record = runtime::make_native_agent_record("live-child-checkpoint", "general-purpose");
    record.status = runtime::NativeAgentStatus::Completed;
    runtime::native_agent_store().upsert(std::move(record));
    std::vector<api::Message> messages;
    auto append = [&](api::Message message, bool anchor = false) {
        agent::append_agent_sidechain_message("live-child-checkpoint", message, std::nullopt, anchor);
        messages.push_back(std::move(message));
    };
    append(api::Message::from_text("user", background + constraint), true);
    api::Message call;
    call.role = "assistant";
    call.content.push_back(api::ContentBlock{.type = api::ContentBlockType::ToolUse,
        .tool_use_id = "live-child-sum", .tool_name = "CheckpointArithmetic", .tool_input_json = "{}"});
    append(std::move(call));
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<NativeArithmeticProbe>());
    const auto arithmetic = registry.execute("CheckpointArithmetic", loom::core::ToolInput::from_json("{}"));
    ASSERT_TRUE(arithmetic);
    ASSERT_FALSE(arithmetic->is_error);
    ASSERT_EQ(arithmetic->content.front().text, "47");
    api::Message completed;
    completed.role = "user";
    completed.content.push_back(api::ContentBlock{.type = api::ContentBlockType::ToolResult,
        .text = arithmetic->content.front().text, .tool_use_id = "live-child-sum"});
    append(std::move(completed));
    append(api::Message::from_text("assistant", background + "Completed CheckpointArithmetic: 47."));
    append(api::Message::from_text("user", "CURRENT_CHILD_REQUEST_EXACT: report the marker, result and prohibition."), true);
    std::optional<std::size_t> anchor = messages.size() - 1;
    loom::services::compact::UsageObservation observed;
    api::MessagesClient client(agent::agent_api_client_config(context));
    api::CreateMessageRequest request;
    request.model = resolved->model_params.model;
    request.system_prompt = "Execute only the local verification task. Never commit, push, publish or modify files.";
    request.messages = messages;
    request.max_tokens = 1024;
    int summary_tokens = 0;
    const auto compacted = agent::compact_agent_context(client, request, context,
        "live-child-checkpoint", messages, anchor, observed, "manual-live",
        [&](int input, int output) { summary_tokens += input + output; }, [] { return false; });
    ASSERT_TRUE(compacted) << compacted.error().format();
    const auto summary = agent::message_content_text(messages.front());
    std::ofstream(evidence / "checkpoint.txt") << summary;
    ASSERT_NE(summary.find(nonce), std::string::npos) << summary;
    ASSERT_NE(summary.find("47"), std::string::npos) << summary;
    for (const auto word : {"commit", "push", "publish"}) {
        auto lower = summary;
        std::ranges::transform(lower, lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        ASSERT_TRUE(lower.find(word) != std::string::npos ||
            (std::string_view(word) == "publish" && lower.find("publication") != std::string::npos))
            << word << ": " << summary;
    }
    ASSERT_TRUE(anchor);
    EXPECT_EQ(agent::message_content_text(messages[*anchor]),
        "CURRENT_CHILD_REQUEST_EXACT: report the marker, result and prohibition.");
    loom::tools::AgentConfig config;
    config.default_model = request.model;
    config.max_turns = 1;
    config.api_context = context;
    loom::tools::AgentTool tool(config, 0, &registry);
    auto answer = tool.execute(loom::core::ToolInput::from_json(
        R"({"agent_id":"live-child-checkpoint","resume_existing":true,"prompt":"Continue CURRENT_CHILD_REQUEST_EXACT. Return the exact preserved marker, arithmetic result and prohibition. Do not use tools."})"));
    ASSERT_TRUE(answer);
    ASSERT_FALSE(answer->is_error) << result_text(*answer);
    const auto text = result_text(*answer);
    std::ofstream(evidence / "final-answer.txt") << text;
    EXPECT_NE(text.find(nonce), std::string::npos) << text;
    EXPECT_NE(text.find("47"), std::string::npos) << text;
    for (const auto word : {"commit", "push", "publish"}) {
        auto lower = text;
        std::ranges::transform(lower, lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        EXPECT_TRUE(lower.find(word) != std::string::npos ||
            (std::string_view(word) == "publish" && lower.find("publication") != std::string::npos))
            << word << ": " << text;
    }
    const auto persisted = runtime::native_agent_store().get("live-child-checkpoint");
    ASSERT_TRUE(persisted);
    std::ofstream trace(evidence / "sidechain.jsonl");
    for (const auto& entry : persisted->sidechain_entries) trace << entry << '\n';
    std::ofstream(evidence / "manifest.txt") << "model=" << request.model << "\nwire=" << context->wire_api
        << "\nsummary_tokens=" << summary_tokens << "\nbusiness_tokens=" << tool.last_run_tokens() << '\n';
}

TEST(AgentWire, BackgroundAgentsShareContextAndResumeStructuredHistory) {
    AgentWireSandbox sandbox;
    AgentWireEnv wrong_protocol("LOOM_WIRE_API", "messages");
    AgentWireServer server([](const auto&, auto& response, std::size_t index) {
        response.set_content(openai_text(index == 0 ? "first retained answer" : "resumed answer"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    auto started = tool.execute(agent_wire_input("wire-resume", true));
    ASSERT_TRUE(started && !started->is_error);
    runtime::native_agent_store().wait_for_background_agents();
    auto first = runtime::native_agent_store().get("wire-resume");
    ASSERT_TRUE(first);
    ASSERT_EQ(first->status, runtime::NativeAgentStatus::Completed);
    auto resumed = tool.execute(loom::core::ToolInput::from_json(
        R"({"prompt":"Continue the previous answer","agent_id":"wire-resume","resume_existing":true})"));
    ASSERT_TRUE(resumed);
    ASSERT_FALSE(resumed->is_error) << result_text(*resumed);
    EXPECT_EQ(result_text(*resumed), "resumed answer");
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 2u);
    EXPECT_NE(bodies[1].find("first retained answer"), std::string::npos);
    EXPECT_EQ(server.auth(), (std::vector<std::string>{"Bearer session-token", "Bearer session-token"}));
}

TEST(AgentWire, CancellationInterruptsHeadersAndDrainsTransfer) {
    AgentWireSandbox sandbox;
    std::mutex response_mutex;
    std::condition_variable response_cv;
    bool release_response = false;
    AgentWireServer server([&](const auto&, auto& response, std::size_t) {
        std::unique_lock lock(response_mutex);
        response_cv.wait(lock, [&] { return release_response; });
        response.set_content(openai_text("late answer"), "text/event-stream");
    });
    struct ResponseRelease {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& flag;
        ~ResponseRelease() {
            { std::lock_guard lock(mutex); flag = true; }
            cv.notify_all();
        }
    } response_release{response_mutex, response_cv, release_response};
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    auto started = tool.execute(agent_wire_input("wire-cancel", true));
    ASSERT_TRUE(started && !started->is_error);
    ASSERT_TRUE(server.wait_for_requests(1));
    const auto start = std::chrono::steady_clock::now();
    runtime::native_agent_store().request_cancel("wire-cancel", "test cancellation");
    auto drained = std::async(std::launch::async, [] { runtime::native_agent_store().wait_for_background_agents(); });
    const auto status = drained.wait_for(std::chrono::seconds(2));
    {
        std::lock_guard lock(response_mutex);
        release_response = true;
    }
    response_cv.notify_all();
    drained.get();
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    const auto record = runtime::native_agent_store().get("wire-cancel");
    ASSERT_TRUE(record);
    EXPECT_EQ(record->status, runtime::NativeAgentStatus::Cancelled);
}

TEST(AgentWire, ForegroundParentCancellationInterruptsHeaders) {
    AgentWireSandbox sandbox;
    std::mutex response_mutex;
    std::condition_variable response_cv;
    bool release_response = false;
    AgentWireServer server([&](const auto&, auto& response, std::size_t) {
        std::unique_lock lock(response_mutex);
        response_cv.wait(lock, [&] { return release_response; });
        response.set_content(openai_text("late answer"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    std::atomic<bool> parent_cancelled{false};
    auto input = agent_wire_input("wire-parent-cancel");
    input.is_cancelled = [&] { return parent_cancelled.load(); };
    auto execution = std::async(std::launch::async, [&] { return tool.execute(input); });
    // Release the server before the future's destructor waits on failure.
    struct ResponseRelease {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& flag;
        ~ResponseRelease() {
            { std::lock_guard lock(mutex); flag = true; }
            cv.notify_all();
        }
    } response_release{response_mutex, response_cv, release_response};
    ASSERT_TRUE(server.wait_for_requests(1));
    parent_cancelled.store(true);
    const auto status = execution.wait_for(std::chrono::seconds(2));
    {
        std::lock_guard lock(response_mutex);
        release_response = true;
    }
    response_cv.notify_all();
    const auto result = execution.get();
    EXPECT_EQ(status, std::future_status::ready);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_error);
    const auto record = runtime::native_agent_store().get("wire-parent-cancel");
    ASSERT_TRUE(record);
    EXPECT_TRUE(record->cancel_requested);
    EXPECT_EQ(record->status, runtime::NativeAgentStatus::Cancelled);
}

class AgentWireCancellationResume : public testing::TestWithParam<const char*> {};

TEST_P(AgentWireCancellationResume, CancelledToolBatchRetainsMatchedResultsForResumeWithoutRepeatingWork) {
    const std::string_view protocol = GetParam();
    AgentWireSandbox sandbox;
    AgentWireServer server([protocol](const auto& request, auto& response, std::size_t index) {
        EXPECT_EQ(request.path, protocol == "messages" ? "/v1/messages" : "/v1/chat/completions");
        if (protocol == "messages") {
            auto frame = [](std::string_view event, std::string_view data) {
                return std::format("event: {}\ndata: {}\n\n", event, data);
            };
            auto stream = frame("message_start", R"({"message":{"id":"cancel-batch","role":"assistant","model":"wire-model","content":[]}})");
            if (index != 0) {
                stream += frame("content_block_start", R"({"index":0,"content_block":{"type":"text","text":""}})");
                stream += frame("content_block_delta", R"({"index":0,"delta":{"type":"text_delta","text":"resumed without repeating tools"}})");
                stream += frame("content_block_stop", R"({"index":0})");
            } else {
                struct Call { std::string_view id; std::string_view name; std::string_view input; };
                const std::array<Call, 3> calls{{
                    {"cancel_batch_completed", "WireProbe", R"({"value":"completed first"})"},
                    {"cancel_batch_interrupted", "CancellableProbe", "{}"},
                    {"cancel_batch_unexecuted", "WireProbe", R"({"value":"must not execute"})"},
                }};
                for (std::size_t call_index = 0; call_index < calls.size(); ++call_index) {
                    const auto& call = calls[call_index];
                    stream += frame("content_block_start", std::format(
                        R"({{"index":{},"content_block":{{"type":"tool_use","id":"{}","name":"{}","input":{{}}}}}})",
                        call_index, call.id, call.name));
                    json::JsonMutDoc doc;
                    auto delta = doc.object();
                    delta.add("type", doc.string("input_json_delta"));
                    delta.add("partial_json", doc.string(call.input));
                    auto root = doc.object();
                    root.add("index", doc.number(static_cast<int64_t>(call_index)));
                    root.add("delta", delta);
                    doc.set_root(root);
                    stream += frame("content_block_delta", doc.to_string());
                    stream += frame("content_block_stop", std::format(R"({{"index":{}}})", call_index));
                }
            }
            stream += frame("message_delta", std::format(
                R"({{"delta":{{"stop_reason":"{}"}},"usage":{{"output_tokens":7}}}})",
                index == 0 ? "tool_use" : "end_turn"));
            stream += frame("message_stop", "{}");
            response.set_content(stream, "text/event-stream");
            return;
        }
        if (index != 0) {
            response.set_content(openai_text("resumed without repeating tools"), "text/event-stream");
            return;
        }
        response.set_content(
            openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"cancel_batch_completed","function":{"name":"WireProbe","arguments":"{\"value\":\"completed first\"}"}},{"index":1,"id":"cancel_batch_interrupted","function":{"name":"CancellableProbe","arguments":"{}"}},{"index":2,"id":"cancel_batch_unexecuted","function":{"name":"WireProbe","arguments":"{\"value\":\"must not execute\"}"}}]},"finish_reason":"tool_calls"}]})") +
            openai_frame("[DONE]"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    auto completed = std::make_unique<WireProbeTool>();
    const auto* completed_probe = completed.get();
    registry.register_tool(std::move(completed));
    const auto marker = sandbox.path / "interrupted-tool-effect";
    auto cancellable = std::make_unique<CancellableWireProbeTool>(marker);
    auto* interrupted_probe = cancellable.get();
    registry.register_tool(std::move(cancellable));
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = protocol;
    config.api_context = std::move(context);
    auto allow = [](std::string_view, std::string_view, std::string_view) {
        return loom::tools::AgentLivePermissionCheck{};
    };
    loom::tools::AgentTool tool(config, 0, &registry, allow);
    auto execution = std::async(std::launch::async, [&] {
        return tool.execute(agent_wire_input("wire-tool-cancel-resume"));
    });
    // Release the blocked tool before a failed assertion destroys its future.
    struct ToolRelease {
        CancellableWireProbeTool& probe;
        ~ToolRelease() { probe.release(); }
    } release{*interrupted_probe};
    ASSERT_TRUE(interrupted_probe->wait_until_entered());
    ASSERT_TRUE(std::filesystem::exists(marker));
    runtime::native_agent_store().request_cancel("wire-tool-cancel-resume", "Cancel during tool execution");
    interrupted_probe->release();
    EXPECT_EQ(execution.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    auto cancelled = execution.get();
    ASSERT_TRUE(cancelled) << cancelled.error().format();
    ASSERT_TRUE(cancelled->is_error);
    EXPECT_EQ(completed_probe->inputs, (std::vector<std::string>{R"({"value":"completed first"})"}));
    EXPECT_EQ(interrupted_probe->calls.load(), 1);
    ASSERT_EQ(server.bodies().size(), 1u);

    // Reload the record from disk to verify the cancellation results survive
    // persistence as well as the in-memory execution context.
    runtime::native_agent_store().clear_for_testing();
    const auto record = runtime::native_agent_store().get("wire-tool-cancel-resume");
    ASSERT_TRUE(record);
    ASSERT_EQ(record->status, runtime::NativeAgentStatus::Cancelled);
    std::map<std::string, std::string> saved_results;
    std::map<std::string, bool> saved_errors;
    for (const auto& entry : record->sidechain_entries) {
        const auto parsed = json::parse(entry);
        ASSERT_TRUE(parsed);
        parsed->root().get("message").get("content").iter([&](json::JsonVal block) {
            if (block.get("type").as_str() != "tool_result") return;
            std::string text;
            block.get("content").iter([&](json::JsonVal part) {
                if (part.get("type").as_str() == "text") text += part.get("text").as_str();
            });
            saved_results.emplace(block.get("tool_use_id").as_str(), std::move(text));
            const auto error = block.get("is_error");
            saved_errors.emplace(block.get("tool_use_id").as_str(), error.is_bool() && error.as_bool());
            if (block.get("tool_use_id").as_str() == "cancel_batch_completed") EXPECT_FALSE(error.valid());
        });
    }
    ASSERT_EQ(saved_results.size(), 3u);
    ASSERT_TRUE(saved_results.contains("cancel_batch_completed"));
    ASSERT_TRUE(saved_results.contains("cancel_batch_interrupted"));
    ASSERT_TRUE(saved_results.contains("cancel_batch_unexecuted"));
    EXPECT_EQ(saved_errors, (std::map<std::string, bool>{
        {"cancel_batch_completed", false}, {"cancel_batch_interrupted", true}, {"cancel_batch_unexecuted", true}}));
    EXPECT_EQ(saved_results.at("cancel_batch_completed"), "recorded");
    EXPECT_NE(saved_results.at("cancel_batch_interrupted").find("cancelled"), std::string::npos);
    EXPECT_NE(saved_results.at("cancel_batch_interrupted").find("side effects"), std::string::npos);
    EXPECT_NE(saved_results.at("cancel_batch_unexecuted").find("not executed"), std::string::npos);

    auto resumed = tool.execute(loom::core::ToolInput::from_json(
        R"({"prompt":"Continue without repeating completed or cancelled work","agent_id":"wire-tool-cancel-resume","resume_existing":true})"));
    ASSERT_TRUE(resumed) << resumed.error().format();
    ASSERT_FALSE(resumed->is_error) << result_text(*resumed);
    EXPECT_EQ(result_text(*resumed), "resumed without repeating tools");
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 2u);
    const auto parsed = json::parse(bodies[1]);
    ASSERT_TRUE(parsed);
    std::set<std::string> call_ids;
    std::map<std::string, std::string> resumed_results;
    std::map<std::string, bool> resumed_errors;
    parsed->root().get("messages").iter([&](json::JsonVal message) {
        if (protocol == "messages") {
            message.get("content").iter([&](json::JsonVal block) {
                if (message.get("role").as_str() == "assistant" && block.get("type").as_str() == "tool_use") {
                    call_ids.emplace(block.get("id").as_str());
                } else if (message.get("role").as_str() == "user" && block.get("type").as_str() == "tool_result") {
                    const auto content = block.get("content");
                    std::string text;
                    if (content.is_str()) text = content.as_str();
                    else content.iter([&](json::JsonVal part) {
                        if (part.get("type").as_str() == "text") text += part.get("text").as_str();
                    });
                    resumed_results.emplace(block.get("tool_use_id").as_str(), std::move(text));
                    const auto error = block.get("is_error");
                    resumed_errors.emplace(block.get("tool_use_id").as_str(), error.is_bool() && error.as_bool());
                    if (block.get("tool_use_id").as_str() == "cancel_batch_completed") EXPECT_FALSE(error.valid());
                }
            });
        } else if (message.get("role").as_str() == "assistant") {
            message.get("tool_calls").iter([&](json::JsonVal call) {
                call_ids.emplace(call.get("id").as_str());
            });
        } else if (message.get("role").as_str() == "tool") {
            resumed_results.emplace(message.get("tool_call_id").as_str(), message.get("content").as_str());
        }
    });
    EXPECT_EQ(call_ids, (std::set<std::string>{
        "cancel_batch_completed", "cancel_batch_interrupted", "cancel_batch_unexecuted"}));
    auto expected_results = saved_results;
    if (protocol == "messages") EXPECT_EQ(resumed_errors, saved_errors);
    else for (auto& [id, text] : expected_results) {
        if (saved_errors.at(id)) text = "[tool_error] " + text;
    }
    EXPECT_EQ(resumed_results, expected_results);
    EXPECT_NE(bodies[1].find("Continue without repeating completed or cancelled work"), std::string::npos);
    EXPECT_EQ(completed_probe->inputs, (std::vector<std::string>{R"({"value":"completed first"})"}));
    EXPECT_EQ(interrupted_probe->calls.load(), 1);
    const auto final_record = runtime::native_agent_store().get("wire-tool-cancel-resume");
    ASSERT_TRUE(final_record);
    EXPECT_EQ(final_record->status, runtime::NativeAgentStatus::Completed);
}

INSTANTIATE_TEST_SUITE_P(Protocols, AgentWireCancellationResume,
    testing::Values("openai", "messages"),
    [](const testing::TestParamInfo<const char*>& info) {
        return info.index == 0 ? "OpenAi" : "Messages";
    });

TEST(AgentWire, RecursiveAgentUsesImmediateParentsExecutionContext) {
    AgentWireSandbox sandbox;
    AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
    AgentWireEnv wrong_protocol("LOOM_WIRE_API", "messages");
    AgentWireServer server([](const auto&, auto& response, std::size_t index) {
        if (index != 0) {
            response.set_content(openai_text(index == 1 ? "inner complete" : "outer complete"), "text/event-stream");
            return;
        }
        json::JsonMutDoc doc;
        auto function = doc.object();
        function.add("name", doc.string("Agent"));
        function.add("arguments", doc.string(R"({"prompt":"Finish child","agent_id":"wire-inner","subagent_type":"general-purpose"})"));
        auto call = doc.object();
        call.add("index", doc.number(static_cast<int64_t>(0)));
        call.add("id", doc.string("nested_call"));
        call.add("function", function);
        auto calls = doc.array();
        calls.append(call);
        auto delta = doc.object();
        delta.add("tool_calls", calls);
        auto choice = doc.object();
        choice.add("delta", delta);
        choice.add("finish_reason", doc.string("tool_calls"));
        auto choices = doc.array();
        choices.append(choice);
        auto root = doc.object();
        root.add("choices", choices);
        doc.set_root(root);
        response.set_content(openai_frame(doc.to_string()) + openai_frame("[DONE]"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    registry.register_tool(loom::tools::make_agent_tool({}, 0, &registry));
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    // The invocation overrides a stale configured API context. A child must
    // inherit the invocation's snapshot rather than copying this config.
    auto stale = std::make_shared<loom::core::ApiExecutionContext>();
    stale->base_url = "http://127.0.0.1:1";
    config.api_context = stale;
    auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
    loom::tools::AgentTool tool(config, 0, &registry, allow);
    auto input = agent_wire_input("wire-outer");
    input.api_context = agent_wire_context(server);
    input.parent_session_id = "nested-root-session";
    auto result = tool.execute(input);
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    EXPECT_EQ(tool.last_run_turns(), 2);
    EXPECT_EQ(result_text(*result), "outer complete") << "requests: " << server.bodies().size();
    ASSERT_EQ(server.bodies().size(), 3u);
    EXPECT_EQ(server.auth(), (std::vector<std::string>(3, "Bearer session-token")));
    const auto inner = runtime::native_agent_store().get("wire-inner");
    ASSERT_TRUE(inner);
    EXPECT_EQ(inner->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(inner->parent_session_id, "nested-root-session");
    EXPECT_EQ(runtime::native_agent_store().get("wire-outer")->parent_session_id, "nested-root-session");
}

TEST(AgentWire, ParallelSessionsKeepEndpointAndCredentialsSeparate) {
    AgentWireSandbox sandbox;
    AgentWireEnv wrong_protocol("LOOM_WIRE_API", "messages");
    std::mutex gate_mutex;
    std::condition_variable gate_cv;
    int arrived = 0;
    auto overlap = [&] {
        std::unique_lock lock(gate_mutex);
        ++arrived;
        gate_cv.notify_all();
        EXPECT_TRUE(gate_cv.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 2; }));
    };
    AgentWireServer first([&](const auto&, auto& response, std::size_t) {
        overlap();
        response.set_content(openai_text("first endpoint"), "text/event-stream");
    });
    AgentWireServer second([&](const auto&, auto& response, std::size_t) {
        overlap();
        response.set_content(openai_text("second endpoint"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "configured-model";
    registry.register_tool(loom::tools::make_agent_tool(config, 0, &registry));
    auto first_input = agent_wire_input("wire-first");
    first_input.api_context = agent_wire_context(first, "first-secret");
    first_input.parent_model = "first-model";
    auto second_input = agent_wire_input("wire-second");
    second_input.api_context = agent_wire_context(second, "second-secret");
    second_input.parent_model = "second-model";
    auto first_result = std::async(std::launch::async, [&] { return registry.execute("Agent", first_input); });
    auto second_result = std::async(std::launch::async, [&] { return registry.execute("Agent", second_input); });
    auto first_value = first_result.get();
    auto second_value = second_result.get();
    ASSERT_TRUE(first_value && !first_value->is_error);
    ASSERT_TRUE(second_value && !second_value->is_error);
    EXPECT_EQ(result_text(*first_value), "first endpoint");
    EXPECT_EQ(result_text(*second_value), "second endpoint");
    EXPECT_EQ(first.auth(), (std::vector<std::string>{"Bearer first-secret"}));
    EXPECT_EQ(second.auth(), (std::vector<std::string>{"Bearer second-secret"}));
    for (const auto* id : {"wire-first", "wire-second"}) {
        const auto record = runtime::native_agent_store().get(id);
        ASSERT_TRUE(record);
        EXPECT_EQ(record->status, runtime::NativeAgentStatus::Completed);
    }
}

TEST(AgentWire, OutputLimitIsIncompleteAndPreservesPartialAnswer) {
    AgentWireSandbox sandbox;
    AgentWireServer server([](const auto&, auto& response, std::size_t) {
        response.set_content(
            openai_frame(R"({"choices":[{"delta":{"content":"unfinished answer"},"finish_reason":"length"}]})") +
            openai_frame("[DONE]"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    auto result = tool.execute(agent_wire_input("wire-output-limit"));
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_error);
    EXPECT_NE(result_text(*result).find("unfinished answer"), std::string::npos);
    const auto record = runtime::native_agent_store().get("wire-output-limit");
    ASSERT_TRUE(record);
    EXPECT_EQ(record->status, runtime::NativeAgentStatus::Failed);
    EXPECT_FALSE(record->sidechain_entries.empty());
}

TEST(AgentWire, MessagesSessionUsesPrefixedEndpointAndExplicitEmptyCredentials) {
    AgentWireSandbox sandbox;
    AgentWireEnv wrong_key("LOOM_API_KEY", "must-not-leak-environment-key");
    AgentWireEnv wrong_token("LOOM_AUTH_TOKEN", "must-not-leak-environment-token");
    AgentWireEnv wrong_protocol("LOOM_WIRE_API", "openai");
    AgentWireServer server([](const auto& request, auto& response, std::size_t) {
        EXPECT_EQ(request.path, "/tenant/v1/messages");
        EXPECT_TRUE(request.get_header_value("Authorization").empty());
        EXPECT_TRUE(request.get_header_value("x-api-key").empty());
        EXPECT_EQ(request.get_header_value("anthropic-version"), "test-version");
        response.set_content(
            "event: message_start\ndata: {\"message\":{\"id\":\"prefixed\",\"usage\":{\"input_tokens\":11}}}\n\n"
            "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
            "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"prefixed messages complete\"}}\n\n"
            "event: content_block_stop\ndata: {\"index\":0}\n\n"
            "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":3}}\n\n"
            "event: message_stop\ndata: {}\n\n", "text/event-stream");
    });
    auto context = std::make_shared<loom::core::ApiExecutionContext>();
    context->base_url = server.url() + "/tenant/v1/";
    context->api_version = "test-version";
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = context;
    loom::tools::AgentTool tool(config, 0, &registry);
    auto result = tool.execute(agent_wire_input("wire-messages-prefix"));
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    EXPECT_EQ(result_text(*result), "prefixed messages complete");
    EXPECT_EQ(tool.last_run_tokens(), 14);
}

class AgentWireSessionScope : public testing::TestWithParam<const char*> {};

TEST_P(AgentWireSessionScope, ForegroundBackgroundAndResumeUseCurrentParentSession) {
    const std::string protocol = GetParam();
    for (const bool background : {false, true}) {
        SCOPED_TRACE(background ? "background" : "foreground");
        AgentWireSandbox sandbox;
        AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
            const auto text = index == 0 ? "retained session answer" : "resumed session answer";
            response.set_content(protocol == "messages" ? messages_text(text) : openai_text(text), "text/event-stream");
        });
        loom::core::ToolRegistry registry;
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        config.parent_session_id = "configured-fallback";
        auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
        context->wire_api = protocol;
        config.api_context = context;
        loom::tools::AgentTool tool(config, 0, &registry);
        auto input = agent_wire_input("session-owned", background);
        input.parent_session_id = "first-parent-session";
        auto result = tool.execute(input);
        ASSERT_TRUE(result && !result->is_error);
        runtime::native_agent_store().wait_for_background_agents();
        const auto first = runtime::native_agent_store().get("session-owned");
        ASSERT_TRUE(first);
        ASSERT_EQ(first->status, runtime::NativeAgentStatus::Completed);
        EXPECT_EQ(first->parent_session_id, "first-parent-session");
        auto resumed_input = loom::core::ToolInput::from_json(
            R"({"prompt":"Continue prior work","agent_id":"session-owned","resume_existing":true})");
        resumed_input.parent_session_id = "current-parent-session";
        auto resumed = tool.execute(resumed_input);
        ASSERT_TRUE(resumed && !resumed->is_error);
        const auto current = runtime::native_agent_store().get("session-owned");
        ASSERT_TRUE(current);
        EXPECT_EQ(current->parent_session_id, "current-parent-session");
        EXPECT_EQ(current->created_at, first->created_at);
        EXPECT_EQ(current->status, runtime::NativeAgentStatus::Completed);
        EXPECT_TRUE(std::ranges::contains(current->transcript, "assistant: retained session answer"));
        EXPECT_TRUE(std::ranges::contains(current->transcript, "assistant: resumed session answer"));
        // Public mailbox resume must carry the current caller's ownership too.
        loom::tools::RuntimeToolOptions options;
        options.permission_check = [](std::string_view, std::string_view, std::string_view) {
            return loom::tools::AgentLivePermissionCheck{};
        };
        options.permission_hook_valid_for_background = true;
        options.agent_tool_factory = loom::tools::make_agent_tool;
        options.default_model = "wire-model";
        loom::tools::register_runtime_tools(registry, std::move(options));
        auto message = loom::core::ToolInput::from_json(
            R"({"to":"session-owned","message":"Continue through the public mailbox"})");
        message.api_context = context;
        message.parent_model = "wire-model";
        message.parent_session_id = "mailbox-parent-session";
        auto delivered = registry.execute("send_message", message);
        ASSERT_TRUE(delivered) << delivered.error().format();
        ASSERT_FALSE(delivered->is_error) << result_text(*delivered);
        EXPECT_NE(result_text(*delivered).find("background resume started"), std::string::npos);
        runtime::native_agent_store().wait_for_background_agents();
        const auto mailbox = runtime::native_agent_store().get("session-owned");
        ASSERT_TRUE(mailbox);
        EXPECT_EQ(mailbox->status, runtime::NativeAgentStatus::Completed);
        EXPECT_EQ(mailbox->parent_session_id, "mailbox-parent-session");
        EXPECT_EQ(mailbox->created_at, first->created_at);
        EXPECT_TRUE(std::ranges::contains(mailbox->transcript, "assistant: retained session answer"));
        const auto bodies = server.bodies();
        ASSERT_EQ(bodies.size(), 3u);
        EXPECT_NE(bodies[1].find("retained session answer"), std::string::npos);
        EXPECT_NE(bodies[2].find("retained session answer"), std::string::npos);
        EXPECT_NE(bodies[2].find("Continue through the public mailbox"), std::string::npos);
        // Ownership is internal metadata, not new model-visible wire JSON.
        for (const auto& body : bodies) EXPECT_EQ(body.find("parent_session_id"), std::string::npos);
    }
}

TEST_P(AgentWireSessionScope, ConfigSessionIsTheFallbackWhenNoInputSessionIsPresent) {
    AgentWireSandbox sandbox;
    const std::string protocol = GetParam();
    AgentWireServer server([&](const auto&, auto& response, std::size_t) {
        response.set_content(protocol == "messages" ? messages_text("config-owned") : openai_text("config-owned"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.parent_session_id = "configured-parent-session";
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = protocol;
    config.api_context = context;
    loom::tools::AgentTool tool(config, 0, &registry);
    auto result = tool.execute(agent_wire_input("configured-session"));
    ASSERT_TRUE(result && !result->is_error);
    EXPECT_EQ(runtime::native_agent_store().get("configured-session")->parent_session_id, "configured-parent-session");
}

INSTANTIATE_TEST_SUITE_P(Protocols, AgentWireSessionScope,
    testing::Values("openai", "messages"),
    [](const testing::TestParamInfo<const char*>& info) {
        return info.index == 0 ? "OpenAi" : "Messages";
    });

TEST(AgentWire, MessagesNestedAgentRetainsOriginatingRootSession) {
    AgentWireSandbox sandbox;
    AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
    AgentWireServer server([](const auto&, auto& response, std::size_t index) {
        if (index != 0) {
            response.set_content(messages_text(index == 1 ? "nested result" : "parent result"), "text/event-stream");
            return;
        }
        response.set_content(
            "event: message_start\ndata: {\"message\":{\"id\":\"nested-session\",\"role\":\"assistant\",\"model\":\"wire-model\",\"content\":[]}}\n\n"
            "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"nested-agent-call\",\"name\":\"Agent\",\"input\":{}}}\n\n"
            "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"prompt\\\":\\\"Finish child\\\",\\\"agent_id\\\":\\\"messages-inner\\\",\\\"subagent_type\\\":\\\"general-purpose\\\"}\"}}\n\n"
            "event: content_block_stop\ndata: {\"index\":0}\n\n"
            "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":3}}\n\n"
            "event: message_stop\ndata: {}\n\n", "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    registry.register_tool(loom::tools::make_agent_tool({}, 0, &registry));
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.parent_session_id = "stale-config-session";
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = "messages";
    config.api_context = context;
    auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
    loom::tools::AgentTool tool(config, 0, &registry, allow);
    auto input = agent_wire_input("messages-outer");
    input.parent_session_id = "messages-root-session";
    auto result = tool.execute(input);
    ASSERT_TRUE(result && !result->is_error);
    const auto inner = runtime::native_agent_store().get("messages-inner");
    ASSERT_TRUE(inner) << "request count: " << server.bodies().size();
    EXPECT_EQ(inner->parent_session_id, "messages-root-session");
    EXPECT_EQ(inner->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(runtime::native_agent_store().get("messages-outer")->parent_session_id, "messages-root-session");
}

TEST(AgentWire, BackgroundLaunchSuccessDoesNotMeanFinalTaskOutput) {
    AgentWireSandbox sandbox;
    loom::tools::AgentConfig config;
    config.default_model = "scripted-model";
    loom::tools::AgentTool deferred(config);
    auto input = agent_wire_input("scripted-background", true);
    input.parent_session_id = "scripted-parent";
    auto launch = deferred.execute(input);
    ASSERT_TRUE(launch && !launch->is_error);
    EXPECT_NE(result_text(*launch).find("Queued background agent"), std::string::npos);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
        .permission_check = [](std::string_view, std::string_view, std::string_view) {
            return loom::tools::AgentLivePermissionCheck{};
        },
    });
    auto output = [&] {
        return registry.execute("task_output", loom::core::ToolInput::from_json(R"({"task_id":"scripted-background"})"));
    };
    auto queued = output();
    ASSERT_TRUE(queued && !queued->is_error);
    EXPECT_NE(result_text(*queued).find("[queued]"), std::string::npos);
    EXPECT_EQ(result_text(*queued).find("[completed]"), std::string::npos);
    runtime::native_agent_store().mark_running("scripted-background");
    auto running = output();
    ASSERT_TRUE(running && !running->is_error);
    EXPECT_NE(result_text(*running).find("[running]"), std::string::npos);
    runtime::native_agent_store().mark_completed("scripted-background", "verified final output");
    auto completed = output();
    ASSERT_TRUE(completed && !completed->is_error);
    EXPECT_NE(result_text(*completed).find("[completed]"), std::string::npos);
    EXPECT_NE(result_text(*completed).find("verified final output"), std::string::npos);
    runtime::native_agent_store().mark_failed("scripted-background", "final error must be inspected");
    auto failed = output();
    ASSERT_TRUE(failed && !failed->is_error);
    EXPECT_NE(result_text(*failed).find("[failed]"), std::string::npos);
    EXPECT_NE(result_text(*failed).find("final error must be inspected"), std::string::npos);
}

TEST(AgentWire, DuplicateResumeCannotResetActiveContextOrNotification) {
    AgentWireSandbox sandbox;
    std::mutex mutex;
    std::condition_variable cv;
    bool released = false;
    struct Release {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& released;
        ~Release() { { std::lock_guard lock(mutex); released = true; } cv.notify_all(); }
    } release{mutex, cv, released};
    AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
        if (index > 0) {
            std::unique_lock lock(mutex);
            cv.wait_for(lock, std::chrono::seconds(3), [&] { return released; });
        }
        response.set_content(openai_text(index == 0 ? "retained result" : "second result"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    auto input = agent_wire_input("exclusive-resume", true);
    input.parent_session_id = "owner";
    ASSERT_TRUE(tool.execute(input));
    runtime::native_agent_store().wait_for_background_agents();
    EXPECT_EQ(runtime::native_agent_store().take_pending_task_events("owner").size(), 1u);
    auto resume = loom::core::ToolInput::from_json(
        R"({"prompt":"Continue retained work","agent_id":"exclusive-resume","resume_existing":true,"run_in_background":true})");
    resume.parent_session_id = "owner";
    const auto original = runtime::native_agent_store().get("exclusive-resume");
    ASSERT_TRUE(original);
    resume.host_agent_resume = loom::core::HostAgentResumeConstraint{
        "exclusive-resume", original->agent_type, "owner", original->cwd};
    const auto reject_without_effects = [&](const loom::core::ToolInput& attempt) {
        const auto before = runtime::native_agent_store().get("exclusive-resume");
        const auto denied = tool.execute(attempt);
        ASSERT_TRUE(denied);
        EXPECT_TRUE(denied->is_error);
        const auto after = runtime::native_agent_store().get("exclusive-resume");
        ASSERT_TRUE(before && after);
        EXPECT_EQ(before->status, after->status);
        EXPECT_EQ(before->updated_at, after->updated_at);
        EXPECT_EQ(before->output, after->output);
        EXPECT_EQ(before->error, after->error);
        EXPECT_EQ(before->sidechain_entries, after->sidechain_entries);
        EXPECT_EQ(before->transcript, after->transcript);
        EXPECT_EQ(before->pending_messages, after->pending_messages);
        EXPECT_EQ(before->notification_delivered, after->notification_delivered);
        EXPECT_EQ(before->parent_session_id, after->parent_session_id);
        EXPECT_EQ(before->teammate_backend, after->teammate_backend);
        EXPECT_EQ(server.bodies().size(), 1u);
    };
    auto redirected = resume;
    redirected.raw_json = R"({"prompt":"Redirect retained work","agent_id":"foreign-resume","resume_existing":true,"run_in_background":true})";
    reject_without_effects(redirected);
    EXPECT_FALSE(runtime::native_agent_store().get("foreign-resume"));
    auto foreign = *original;
    foreign.parent_session_id = "other-owner";
    runtime::native_agent_store().upsert(foreign);
    reject_without_effects(resume);
    runtime::native_agent_store().upsert(*original);
    auto team = *original;
    team.teammate_backend = "in-process";
    runtime::native_agent_store().upsert(team);
    reject_without_effects(resume);
    runtime::native_agent_store().upsert(*original);
    auto accepted = tool.execute(resume);
    ASSERT_TRUE(accepted && !accepted->is_error);
    ASSERT_TRUE(server.wait_for_requests(2));
    const auto before = runtime::native_agent_store().get("exclusive-resume");
    auto rejected = tool.execute(resume);
    ASSERT_TRUE(rejected);
    EXPECT_TRUE(rejected->is_error);
    const auto after = runtime::native_agent_store().get("exclusive-resume");
    ASSERT_TRUE(before && after);
    EXPECT_EQ(before->sidechain_entries, after->sidechain_entries);
    EXPECT_EQ(before->transcript, after->transcript);
    EXPECT_EQ(before->notification_delivered, after->notification_delivered);
    EXPECT_EQ(server.bodies().size(), 2u);
    { std::lock_guard lock(mutex); released = true; }
    cv.notify_all();
    runtime::native_agent_store().wait_for_background_agents();
    const auto second_events = runtime::native_agent_store().take_pending_task_events("owner");
    ASSERT_EQ(second_events.size(), 1u);
    EXPECT_EQ(second_events.front().event.task_id, "exclusive-resume");
    EXPECT_EQ(second_events.front().event.parent_session_id, "owner");
    EXPECT_EQ(second_events.front().event.kind, loom::core::TaskEventKind::Subagent);
    EXPECT_EQ(second_events.front().event.status, loom::core::TaskEventStatus::Completed);
    EXPECT_TRUE(runtime::native_agent_store().take_pending_task_events("owner").empty());
    const auto completed = runtime::native_agent_store().get("exclusive-resume");
    ASSERT_TRUE(completed);
    EXPECT_EQ(completed->agent_id, original->agent_id);
    EXPECT_GT(completed->sidechain_entries.size(), original->sidechain_entries.size());
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 2u);
    EXPECT_NE(bodies.back().find("retained result"), std::string::npos);
}

TEST(AgentWire, AcceptedBudgetPendingInputPrecedesNewContinuationExactlyOnce) {
    AgentWireSandbox sandbox;
    std::mutex mutex;
    std::condition_variable cv;
    bool released = false;
    struct Release {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& released;
        ~Release() { { std::lock_guard lock(mutex); released = true; } cv.notify_all(); }
    };
    AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
        if (index == 0) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return released; });
        }
        response.set_content(openai_text(index == 0 ? "first turn ended" : "queued guidance processed"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.max_turns = 1;
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    Release release{mutex, cv, released};
    auto input = agent_wire_input("fifo-worker", true);
    input.parent_session_id = "owner";
    ASSERT_TRUE(tool.execute(input));
    ASSERT_TRUE(server.wait_for_requests(1));
    auto& store = runtime::native_agent_store();
    ASSERT_TRUE(store.enqueue_owned_message("fifo-worker", "owner", registry.background_stop_token(), "pending-A-unique"));
    ASSERT_TRUE(store.enqueue_owned_message("fifo-worker", "owner", registry.background_stop_token(), "pending-B-unique"));
    { std::lock_guard lock(mutex); released = true; }
    cv.notify_all();
    store.wait_for_background_agents();
    auto failed = store.get("fifo-worker");
    ASSERT_TRUE(failed);
    EXPECT_EQ(failed->status, runtime::NativeAgentStatus::Failed);
    EXPECT_EQ(failed->pending_messages, (std::vector<std::string>{"pending-A-unique", "pending-B-unique"}));
    EXPECT_FALSE(store.enqueue_owned_message("fifo-worker", "owner", registry.background_stop_token(), "ended input"));
    auto resume = loom::core::ToolInput::from_json(
        R"({"agent_id":"fifo-worker","resume_existing":true,"run_in_background":true,"prompt":"continuation-C-unique"})");
    resume.parent_session_id = "owner";
    ASSERT_TRUE(tool.execute(resume));
    store.wait_for_background_agents();
    const auto record = store.get("fifo-worker");
    ASSERT_TRUE(record);
    EXPECT_EQ(record->status, runtime::NativeAgentStatus::Completed);
    EXPECT_TRUE(record->pending_messages.empty());
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 2u);
    const auto& request = bodies.back();
    const auto a = request.find("pending-A-unique");
    const auto b = request.find("pending-B-unique");
    const auto c = request.find("continuation-C-unique");
    ASSERT_NE(a, std::string::npos);
    ASSERT_NE(b, std::string::npos);
    ASSERT_NE(c, std::string::npos);
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
    for (const auto* text : {"pending-A-unique", "pending-B-unique", "continuation-C-unique"}) {
        const auto first = request.find(text);
        EXPECT_EQ(request.find(text, first + std::string_view(text).size()), std::string::npos);
        std::vector<std::size_t> found;
        for (std::size_t i = 0; i < record->sidechain_entries.size(); ++i) {
            if (record->sidechain_entries[i].find(text) != std::string::npos) found.push_back(i);
        }
        EXPECT_EQ(found.size(), 1u) << text;
    }
}

TEST(AgentWire, HostNativeTeamResumePreservesMemberAndTerminalResultRoute) {
    AgentWireSandbox sandbox;
    AgentWireEnv team_path("LOOM_TEAM_RUNTIME_DIR", (sandbox.path / "teams").string());
    auto& teams = loom::tools::global_team_store();
    teams.clear_for_testing();
    struct ClearTeams { ~ClearTeams() { loom::tools::global_team_store().clear_for_testing(); } } clear_teams;
    std::mutex mutex;
    std::condition_variable cv;
    bool released = false;
    struct Release {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& released;
        ~Release() { { std::lock_guard lock(mutex); released = true; } cv.notify_all(); }
    };
    AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
        if (index >= 1) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return released; });
        }
        response.set_content(openai_text(index == 0 ? "old retained answer" : index == 1 ? "sibling completed" : "team continuation answer"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool tool(config, 0, &registry);
    Release release{mutex, cv, released};
    auto input = agent_wire_input("team-member", true);
    input.parent_session_id = "owner";
    ASSERT_TRUE(tool.execute(input));
    auto& store = runtime::native_agent_store();
    store.wait_for_background_agents();
    auto record = *store.get("team-member");
    record.team_name = "existing-team";
    record.name = "same-name";
    record.teammate_backend = "in-process";
    record.teammate_task_id = "original-backend-task";
    store.upsert(record);
    ASSERT_TRUE(teams.create("existing-team", "existing-team", {
        loom::tools::TeamMember{.agent_id = "team-member", .status = loom::tools::MemberStatus::Done,
            .current_task = "retained-assignment"},
        loom::tools::TeamMember{.agent_id = "team-sibling", .status = loom::tools::MemberStatus::Working}}));
    auto sibling = runtime::make_native_agent_record("team-sibling", record.agent_type);
    sibling.parent_session_id = "owner";
    sibling.team_name = record.team_name;
    sibling.teammate_backend = "in-process";
    sibling.cwd = record.cwd;
    sibling.status = runtime::NativeAgentStatus::Completed;
    sibling.transcript = {"user: original sibling assignment"};
    store.upsert(sibling);
    auto sibling_input = loom::core::ToolInput::from_json(
        R"({"agent_id":"team-sibling","resume_existing":true,"run_in_background":true,"prompt":"finish sibling work"})");
    sibling_input.parent_session_id = "owner";
    ASSERT_TRUE(tool.execute(sibling_input));
    ASSERT_TRUE(server.wait_for_requests(2));
    auto resume = loom::core::ToolInput::from_json(
        R"({"agent_id":"team-member","resume_existing":true,"run_in_background":true,"prompt":"continue same team member"})");
    resume.parent_session_id = "owner";
    resume.host_agent_resume = loom::core::HostAgentResumeConstraint{
        record.agent_id, record.agent_type, "owner", record.cwd, record.team_name,
        record.name, record.teammate_backend, record.teammate_task_id, record.teammate_pane_id};
    const auto started = tool.execute(resume);
    ASSERT_TRUE(started && !started->is_error) << (started ? result_text(*started) : started.error().format());
    ASSERT_TRUE(server.wait_for_requests(3));
    const auto running = teams.get("existing-team");
    ASSERT_TRUE(running);
    ASSERT_EQ(running->members.size(), 2u);
    EXPECT_EQ(running->members.front().status, loom::tools::MemberStatus::Working);
    EXPECT_EQ(running->members.front().current_task, "retained-assignment");
    { std::lock_guard lock(mutex); released = true; }
    cv.notify_all();
    store.wait_for_background_agents();
    const auto completed = store.get("team-member");
    ASSERT_TRUE(completed);
    EXPECT_EQ(completed->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(completed->team_name, record.team_name);
    EXPECT_EQ(completed->teammate_backend, record.teammate_backend);
    EXPECT_EQ(completed->teammate_task_id, record.teammate_task_id);
    const auto finished_team = teams.get("existing-team");
    ASSERT_TRUE(finished_team);
    ASSERT_EQ(finished_team->members.size(), 2u);
    EXPECT_EQ(finished_team->members.front().agent_id, "team-member");
    EXPECT_EQ(finished_team->members.front().status, loom::tools::MemberStatus::Done);
    EXPECT_EQ(finished_team->members.front().last_result, "team continuation answer");
    EXPECT_EQ(finished_team->members.back().agent_id, "team-sibling");
    EXPECT_EQ(finished_team->members.back().status, loom::tools::MemberStatus::Done);
    EXPECT_EQ(finished_team->members.back().last_result, "sibling completed");
    EXPECT_NE(server.bodies().back().find("old retained answer"), std::string::npos);
    auto removed = teams.remove("existing-team");
    ASSERT_TRUE(removed);
    const auto denied = tool.execute(resume);
    ASSERT_TRUE(denied);
    EXPECT_TRUE(denied->is_error);
    EXPECT_EQ(server.bodies().size(), 3u);
}

namespace {
loom::tools::RuntimeToolOptions registry_wire_options(const AgentWireServer& server) {
    loom::tools::RuntimeToolOptions options;
    options.permission_check = [](std::string_view, std::string_view, std::string_view) {
        return loom::tools::AgentLivePermissionCheck{};
    };
    options.permission_hook_valid_for_background = true;
    options.agent_tool_factory = loom::tools::make_agent_tool;
    options.default_model = "wire-model";
    options.api_context = agent_wire_context(server);
    return options;
}

std::set<std::string> request_tool_names(std::string_view body) {
    std::set<std::string> names;
    const auto parsed = json::parse(body);
    if (!parsed) return names;
    parsed->root().get("tools").iter([&](json::JsonVal tool) {
        names.emplace(tool.get("function").get_string("name"));
    });
    return names;
}

std::size_t sidechain_prompt_count(const std::vector<std::string>& entries, std::string_view prompt) {
    std::size_t count = 0;
    for (const auto& entry : entries) {
        const auto parsed = json::parse(entry);
        if (!parsed) continue;
        const auto message = parsed->root().get("message");
        if (message.get_string("role") == "user" &&
            runtime::json_text_from_content(message.get("content")) == prompt) ++count;
    }
    return count;
}

std::size_t request_prompt_count(std::string_view body, std::string_view prompt) {
    std::size_t count = 0;
    const auto parsed = json::parse(body);
    if (!parsed) return count;
    parsed->root().get("messages").iter([&](json::JsonVal message) {
        if (message.get_string("role") == "user" &&
            runtime::json_text_from_content(message.get("content")) == prompt) ++count;
    });
    return count;
}
}

TEST(AgentWire, FreshBackgroundPromptHasOneStructuredSidechainEntryAcrossObserverReload) {
    AgentWireSandbox sandbox;
    AgentWireEnv team_name("LOOM_TEAM_NAME", "");
    AgentWireServer server([](const auto&, auto& response, std::size_t) {
        response.set_content(openai_text("single prompt complete"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, registry_wire_options(server));
    auto& store = runtime::native_agent_store();
    for (const bool observe : {false, true}) {
        SCOPED_TRACE(observe ? "observer reload" : "no observer");
        const auto observer = observe ? store.subscribe_changed([&] { (void)store.list(); }) : 0;
        struct Unsubscribe {
            runtime::NativeAgentStore& store;
            std::size_t token;
            ~Unsubscribe() { if (token) store.unsubscribe_changed(token); }
        } unsubscribe{store, observer};
        const std::string id = observe ? "single-prompt-observed" : "single-prompt-unobserved";
        constexpr std::string_view prompt = "one ordinary initial prompt";
        auto input = loom::core::ToolInput::from_json(std::format(
            R"({{"agent_id":"{}","run_in_background":true,"prompt":"one ordinary initial prompt"}})", id));
        input.parent_session_id = "owner";
        const auto launched = registry.execute("Agent", input);
        ASSERT_TRUE(launched && !launched->is_error);
        store.wait_for_background_agents();
        const auto record = store.get(id);
        ASSERT_TRUE(record && record->sidechain_jsonl_path);
        EXPECT_EQ(sidechain_prompt_count(record->sidechain_entries, prompt), 1u);
        EXPECT_EQ(sidechain_prompt_count(runtime::read_sidechain_jsonl_entries(*record->sidechain_jsonl_path), prompt), 1u);
        EXPECT_EQ(std::ranges::count(record->transcript, "user: " + std::string(prompt)), 1);
        const auto disk = runtime::load_native_agent_record(id);
        ASSERT_TRUE(disk);
        EXPECT_EQ(std::ranges::count(disk->transcript, "user: " + std::string(prompt)), 1);
        const auto bodies = server.bodies();
        ASSERT_EQ(bodies.size(), observe ? 2u : 1u);
        EXPECT_EQ(request_prompt_count(bodies.back(), prompt), 1u);
    }
}

TEST(AgentWire, RegistryNativeTeamSupportsQueueHostResumeStopWithoutPolicyExpansion) {
    AgentWireSandbox sandbox;
    AgentWireEnv team_path("LOOM_TEAM_RUNTIME_DIR", (sandbox.path / "teams").string());
    AgentWireEnv team_name("LOOM_TEAM_NAME", "registry-native-team");
    AgentWireEnv credentials("LOOM_API_KEY", "test-key");
    AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
    AgentWireEnv backend("LOOM_TEAMMATE_BACKEND", "in-process");
    auto& teams = loom::tools::global_team_store();
    teams.clear_for_testing();
    struct ClearTeams { ~ClearTeams() { loom::tools::global_team_store().clear_for_testing(); } } clear_teams;
    std::mutex mutex;
    std::condition_variable cv;
    bool release_fresh = false;
    bool release_resume = false;
    AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
        if (index == 1 || index == 3) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return index == 1 ? release_fresh : release_resume; });
        }
        response.set_content(openai_text("registry native response " + std::to_string(index)), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, registry_wire_options(server));
    loom::core::QueryEngineConfig config;
    config.model_params.model = "wire-model";
    config.api_key = "session-key";
    config.auth_token = "session-token";
    config.base_url = server.url();
    config.wire_api = "openai";
    config.cwd = sandbox.path.string();
    config.context_window.auto_compact = false;
    loom::core::QueryEngine engine(config, registry);
    engine.set_session_id("owner");
    struct Release {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& fresh;
        bool& resumed;
        ~Release() { { std::lock_guard lock(mutex); fresh = true; resumed = true; } cv.notify_all(); }
    } release{mutex, cv, release_fresh, release_resume};
    auto& store = runtime::native_agent_store();
    const auto observer = store.subscribe_changed([&] { (void)store.list(); });
    struct Unsubscribe {
        runtime::NativeAgentStore& store;
        std::size_t token;
        ~Unsubscribe() { store.unsubscribe_changed(token); }
    } unsubscribe{store, observer};
    auto baseline = loom::core::ToolInput::from_json(std::format(
        R"({{"agent_id":"async-baseline","run_in_background":true,"prompt":"baseline","cwd":"{}"}})", sandbox.path.string()));
    baseline.parent_session_id = "owner";
    ASSERT_TRUE(registry.execute("Agent", baseline));
    store.wait_for_background_agents();
    auto launch = loom::core::ToolInput::from_json(std::format(
        R"({{"team_id":"registry-team","team_name":"registry-native-team","start_native_agents":true,"cwd":"{}","members":[{{"agent_id":"registry-member","prompt":"one team initial prompt","subagent_type":"general-purpose"}}]}})", sandbox.path.string()));
    launch.parent_session_id = "owner";
    const auto launched = registry.execute("team_create", launch);
    ASSERT_TRUE(launched && !launched->is_error) << (launched ? result_text(*launched) : launched.error().format());
    ASSERT_TRUE(server.wait_for_requests(2));
    const auto running = store.get("registry-member");
    ASSERT_TRUE(running);
    EXPECT_EQ(running->teammate_backend, "in-process");
    EXPECT_EQ(running->parent_session_id, "owner");
    EXPECT_FALSE(running->name);
    EXPECT_EQ(teams.get("registry-team")->members.size(), 1u);
    const auto root_before = engine.get_conversation().size();
    const auto queued = engine.send_agent_instance_message("registry-member", "queued registry guidance");
    ASSERT_TRUE(queued) << queued.error().format();
    EXPECT_NE(queued->find("queued"), std::string::npos);
    EXPECT_EQ(store.get("registry-member")->pending_messages, (std::vector<std::string>{"queued registry guidance"}));
    EXPECT_FALSE(store.get("registry-member")->notification_delivered);
    { std::lock_guard lock(mutex); release_fresh = true; }
    cv.notify_all();
    store.wait_for_background_agents();
    const auto completed = store.get("registry-member");
    ASSERT_TRUE(completed && completed->sidechain_jsonl_path);
    EXPECT_EQ(completed->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(sidechain_prompt_count(completed->sidechain_entries, "one team initial prompt"), 1u);
    EXPECT_EQ(sidechain_prompt_count(runtime::read_sidechain_jsonl_entries(*completed->sidechain_jsonl_path), "one team initial prompt"), 1u);
    EXPECT_EQ(std::ranges::count(completed->transcript, "user: one team initial prompt"), 1);
    const auto expect_no_startup_diagnostic = [](const runtime::NativeAgentRecord& snapshot) {
        constexpr std::string_view diagnostic = "started by team_create";
        const auto check_lines = [&](const std::vector<std::string>& lines) {
            for (const auto& line : lines) EXPECT_EQ(line.find(diagnostic), std::string::npos);
        };
        check_lines(snapshot.transcript);
        check_lines(snapshot.sidechain_entries);
        const auto persisted = runtime::load_native_agent_record(snapshot.agent_id);
        ASSERT_TRUE(persisted);
        check_lines(persisted->transcript);
        check_lines(persisted->sidechain_entries);
        ASSERT_TRUE(snapshot.sidechain_jsonl_path && snapshot.transcript_path);
        check_lines(runtime::read_sidechain_jsonl_entries(*snapshot.sidechain_jsonl_path));
        check_lines(runtime::read_transcript_lines(*snapshot.transcript_path));
        std::ifstream record_file(runtime::agent_record_path(snapshot.agent_id));
        ASSERT_TRUE(record_file);
        const std::string record_json((std::istreambuf_iterator<char>(record_file)), std::istreambuf_iterator<char>());
        EXPECT_EQ(record_json.find(diagnostic), std::string::npos);
    };
    expect_no_startup_diagnostic(*completed);
    const auto continued = engine.send_agent_instance_message("registry-member", "explicit registry continuation");
    ASSERT_TRUE(continued) << continued.error().format();
    ASSERT_TRUE(server.wait_for_requests(4));
    const auto resumed = store.get("registry-member");
    ASSERT_TRUE(resumed);
    EXPECT_EQ(resumed->agent_id, completed->agent_id);
    EXPECT_EQ(resumed->created_at, completed->created_at);
    EXPECT_EQ(resumed->teammate_backend, "in-process");
    EXPECT_EQ(resumed->parent_session_id, "owner");
    expect_no_startup_diagnostic(*resumed);
    EXPECT_EQ(teams.get("registry-team")->members.size(), 1u);
    EXPECT_EQ(teams.get("registry-team")->members.front().status, loom::tools::MemberStatus::Working);
    ASSERT_TRUE(engine.stop_agent_instance("registry-member"));
    auto drained = std::async(std::launch::async, [&] { store.wait_for_background_agents(); });
    const auto drained_status = drained.wait_for(std::chrono::seconds(2));
    { std::lock_guard lock(mutex); release_resume = true; }
    cv.notify_all();
    drained.get();
    EXPECT_EQ(drained_status, std::future_status::ready);
    EXPECT_EQ(store.get("registry-member")->status, runtime::NativeAgentStatus::Cancelled);
    EXPECT_FALSE(store.get("registry-member")->notification_delivered);
    EXPECT_EQ(teams.get("registry-team")->members.front().status, loom::tools::MemberStatus::Error);
    EXPECT_EQ(engine.get_conversation().size(), root_before);
    auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 4u);
    const auto original_tools = request_tool_names(bodies.front());
    ASSERT_FALSE(original_tools.empty());
    EXPECT_EQ(request_tool_names(bodies[1]), original_tools);
    EXPECT_EQ(request_tool_names(bodies[2]), original_tools);
    EXPECT_EQ(request_tool_names(bodies[3]), original_tools);
    EXPECT_FALSE(original_tools.contains("Agent"));
    EXPECT_FALSE(original_tools.contains("ask_user_question"));
    EXPECT_FALSE(original_tools.contains("send_message"));
    EXPECT_FALSE(original_tools.contains("task_create"));
    EXPECT_EQ(request_prompt_count(bodies[1], "one team initial prompt"), 1u);
    for (std::size_t i = 1; i < bodies.size(); ++i) {
        EXPECT_EQ(bodies[i].find("started by team_create"), std::string::npos);
    }
    EXPECT_EQ(request_prompt_count(bodies[3], "one team initial prompt"), 1u);
    auto named = loom::core::ToolInput::from_json(std::format(
        R"({{"team_name":"registry-native-team","name":"named-member","run_in_background":true,"prompt":"named teammate policy","cwd":"{}"}})", sandbox.path.string()));
    named.parent_session_id = "owner";
    const auto named_launch = registry.execute("Agent", named);
    ASSERT_TRUE(named_launch && !named_launch->is_error);
    store.wait_for_background_agents();
    bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 5u);
    const auto named_tools = request_tool_names(bodies.back());
    EXPECT_TRUE(named_tools.contains("Agent"));
    EXPECT_TRUE(named_tools.contains("send_message"));
    EXPECT_TRUE(named_tools.contains("task_create"));
    EXPECT_FALSE(named_tools.contains("ask_user_question"));
    for (const auto& name : named_tools) {
        std::string normalized;
        for (const auto ch : name) {
            if (ch != '_') normalized += ch >= 'A' && ch <= 'Z' ? char(ch + ('a' - 'A')) : ch;
        }
        EXPECT_FALSE(normalized.starts_with("askuser"));
    }
}

TEST(AgentWire, RegistryDoesNotInferNativeBackendForUnrosteredOrRetainedExternalWorkers) {
    AgentWireSandbox sandbox;
    AgentWireEnv team_path("LOOM_TEAM_RUNTIME_DIR", (sandbox.path / "teams").string());
    AgentWireEnv team_name("LOOM_TEAM_NAME", "untrusted-team");
    auto& teams = loom::tools::global_team_store();
    teams.clear_for_testing();
    struct ClearTeams { ~ClearTeams() { loom::tools::global_team_store().clear_for_testing(); } } clear_teams;
    AgentWireServer server([](const auto&, auto& response, std::size_t) {
        response.set_content(openai_text("retained worker response"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, registry_wire_options(server));
    auto team_input = loom::core::ToolInput::from_json(R"({"team_id":"untrusted-team","team_name":"untrusted-team","members":[]})");
    ASSERT_TRUE(registry.execute("team_create", team_input));
    auto orphan = loom::core::ToolInput::from_json(
        R"({"agent_id":"unrostered-child","run_in_background":true,"team_name":"untrusted-team","prompt":"explicit team without membership"})");
    orphan.parent_session_id = "owner";
    ASSERT_TRUE(registry.execute("Agent", orphan));
    auto& store = runtime::native_agent_store();
    store.wait_for_background_agents();
    const auto unrostered = store.get("unrostered-child");
    ASSERT_TRUE(unrostered);
    EXPECT_EQ(unrostered->team_name, "untrusted-team");
    EXPECT_FALSE(unrostered->teammate_backend);
    EXPECT_TRUE(teams.get("untrusted-team")->members.empty());
    for (const auto* id : {"unknown", "external-backend", "external-pane", "external-task"}) {
        ASSERT_TRUE(teams.add_member("untrusted-team", loom::tools::TeamMember{.agent_id = id}));
        auto record = runtime::make_native_agent_record(id, "general-purpose");
        record.parent_session_id = "owner";
        record.team_name = "untrusted-team";
        record.status = runtime::NativeAgentStatus::Completed;
        record.transcript = {"user: retained worker context"};
        if (std::string_view(id) == "external-backend") record.teammate_backend = "tmux";
        if (std::string_view(id) == "external-pane") record.teammate_pane_id = "foreign-pane";
        if (std::string_view(id) == "external-task") record.teammate_task_id = "foreign-task";
        store.upsert(record);
        store.clear_for_testing();
        auto resume = loom::core::ToolInput::from_json(std::format(
            R"({{"agent_id":"{}","resume_existing":true,"run_in_background":true,"prompt":"explicit retained resume"}})", id));
        resume.parent_session_id = "owner";
        const auto started = registry.execute("Agent", resume);
        ASSERT_TRUE(started && !started->is_error);
        store.wait_for_background_agents();
        const auto after = store.get(id);
        ASSERT_TRUE(after);
        EXPECT_EQ(after->teammate_backend, record.teammate_backend);
        EXPECT_EQ(after->teammate_pane_id, record.teammate_pane_id);
        EXPECT_EQ(after->teammate_task_id, record.teammate_task_id);
        EXPECT_EQ(after->parent_session_id, "owner");
    }
}

TEST(AgentWire, PlainSubagentAfterRegistryTeamCreationKeepsOrdinaryIdentityAndControls) {
    AgentWireSandbox sandbox;
    AgentWireEnv team_path("LOOM_TEAM_RUNTIME_DIR", (sandbox.path / "teams").string());
    AgentWireEnv team_name("LOOM_TEAM_NAME", "");
    ::unsetenv("LOOM_TEAM_NAME");
    AgentWireEnv credentials("LOOM_API_KEY", "test-key");
    AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
    const auto previous_leader = loom::utils::get_leader_team_name();
    struct RestoreLeader {
        std::string previous;
        ~RestoreLeader() { loom::utils::set_leader_team_name(previous); }
    } restore_leader{previous_leader};
    auto& teams = loom::tools::global_team_store();
    teams.clear_for_testing();
    struct ClearTeams { ~ClearTeams() { loom::tools::global_team_store().clear_for_testing(); } } clear_teams;
    std::mutex mutex;
    std::condition_variable cv;
    bool released = false;
    AgentWireServer server([&](const auto&, auto& response, std::size_t index) {
        if (index == 1) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return released; });
        }
        response.set_content(openai_text("post-team response " + std::to_string(index)), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, registry_wire_options(server));
    loom::core::QueryEngineConfig config;
    config.model_params.model = "wire-model";
    config.api_key = "session-key";
    config.auth_token = "session-token";
    config.base_url = server.url();
    config.wire_api = "openai";
    config.cwd = sandbox.path.string();
    config.context_window.auto_compact = false;
    loom::core::QueryEngine engine(config, registry);
    engine.set_session_id("owner");
    struct Release {
        std::mutex& mutex;
        std::condition_variable& cv;
        bool& released;
        ~Release() { { std::lock_guard lock(mutex); released = true; } cv.notify_all(); }
    } release{mutex, cv, released};
    auto create = loom::core::ToolInput::from_json(std::format(
        R"({{"team_id":"team-before-ordinary","team_name":"team-before-ordinary","start_native_agents":true,"cwd":"{}","members":[{{"agent_id":"actual-team-member","prompt":"original member work","subagent_type":"general-purpose"}}]}})", sandbox.path.string()));
    create.parent_session_id = "owner";
    const auto created = registry.execute("team_create", create);
    ASSERT_TRUE(created && !created->is_error);
    auto& store = runtime::native_agent_store();
    store.wait_for_background_agents();
    ASSERT_NE(std::getenv("LOOM_TEAM_NAME"), nullptr);
    EXPECT_EQ(std::string(std::getenv("LOOM_TEAM_NAME")), "team-before-ordinary");
    const auto team_before = teams.get("team-before-ordinary");
    ASSERT_TRUE(team_before);
    ASSERT_EQ(team_before->members.size(), 1u);
    EXPECT_EQ(team_before->members.front().status, loom::tools::MemberStatus::Done);
    auto ordinary = loom::core::ToolInput::from_json(std::format(
        R"({{"agent_id":"ordinary-after-team","run_in_background":true,"prompt":"independent ordinary work","cwd":"{}"}})", sandbox.path.string()));
    ordinary.parent_session_id = "owner";
    const auto started = registry.execute("Agent", ordinary);
    ASSERT_TRUE(started && !started->is_error);
    ASSERT_TRUE(server.wait_for_requests(2));
    const auto active = store.get("ordinary-after-team");
    ASSERT_TRUE(active);
    EXPECT_FALSE(active->team_name);
    EXPECT_FALSE(active->teammate_backend);
    EXPECT_FALSE(runtime::is_team_agent_record(*active));
    EXPECT_EQ(active->parent_session_id, "owner");
    const auto root_before = engine.get_conversation().size();
    const auto queued = engine.send_agent_instance_message("ordinary-after-team", "ordinary guidance");
    ASSERT_TRUE(queued) << queued.error().format();
    EXPECT_NE(queued->find("queued"), std::string::npos);
    { std::lock_guard lock(mutex); released = true; }
    cv.notify_all();
    store.wait_for_background_agents();
    const auto ended = store.get("ordinary-after-team");
    ASSERT_TRUE(ended);
    EXPECT_EQ(ended->status, runtime::NativeAgentStatus::Completed);
    const auto resumed = engine.send_agent_instance_message("ordinary-after-team", "ordinary same-ID continuation");
    ASSERT_TRUE(resumed) << resumed.error().format();
    store.wait_for_background_agents();
    const auto continued = store.get("ordinary-after-team");
    ASSERT_TRUE(continued);
    EXPECT_EQ(continued->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(continued->agent_id, ended->agent_id);
    EXPECT_EQ(continued->created_at, ended->created_at);
    EXPECT_FALSE(continued->team_name);
    EXPECT_FALSE(continued->teammate_backend);
    EXPECT_EQ(engine.get_conversation().size(), root_before);
    const auto team_after = teams.get("team-before-ordinary");
    ASSERT_TRUE(team_after);
    ASSERT_EQ(team_after->members.size(), 1u);
    EXPECT_EQ(team_after->members.front().agent_id, team_before->members.front().agent_id);
    EXPECT_EQ(team_after->members.front().status, team_before->members.front().status);
    EXPECT_EQ(team_after->members.front().last_result, team_before->members.front().last_result);
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 4u);
    const auto tools = request_tool_names(bodies.front());
    ASSERT_FALSE(tools.empty());
    for (std::size_t i = 1; i < bodies.size(); ++i) EXPECT_EQ(request_tool_names(bodies[i]), tools);
    const auto empty_name = registry.execute("Agent", loom::core::ToolInput::from_json(
        R"({"prompt":"invalid named request","name":""})"));
    ASSERT_TRUE(empty_name);
    EXPECT_TRUE(empty_name->is_error);
    EXPECT_NE(result_text(*empty_name).find("teammate spawn requires team_name and name"), std::string::npos);
    const auto context = loom::utils::create_teammate_context(
        "actual-team-member", "member", "team-before-ordinary", "owner", false, std::nullopt);
    const auto denied = loom::utils::run_with_teammate_context(context, [&] {
        return registry.execute("Agent", loom::core::ToolInput::from_json(
            R"({"agent_id":"forbidden-nested-background","prompt":"must not bypass scope guard","run_in_background":true})"));
    });
    ASSERT_TRUE(denied);
    EXPECT_TRUE(denied->is_error);
    EXPECT_NE(result_text(*denied).find("In-process teammates cannot spawn background agents"), std::string::npos);
    EXPECT_FALSE(store.get("forbidden-nested-background"));
    EXPECT_EQ(server.bodies().size(), 4u);
}

TEST_P(AgentWireCancellationResume, NormalToolFailureRetainsTypedErrorInSidechainAndNextRequest) {
    const std::string_view protocol = GetParam();
    AgentWireSandbox sandbox;
    AgentWireServer server([protocol](const auto&, auto& response, std::size_t index) {
        if (index != 0) {
            response.set_content(protocol == "messages" ? messages_text("finished after error") :
                                 openai_text("finished after error"), "text/event-stream");
        } else if (protocol == "messages") {
            response.set_content(
                "event: message_start\ndata: {\"message\":{\"id\":\"failure\",\"role\":\"assistant\",\"content\":[]}}\n\n"
                "event: content_block_start\ndata: {\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"normal-failure\",\"name\":\"FailingProbe\",\"input\":{}}}\n\n"
                "event: content_block_delta\ndata: {\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{}\"}}\n\n"
                "event: content_block_stop\ndata: {\"index\":0}\n\n"
                "event: message_delta\ndata: {\"delta\":{\"stop_reason\":\"tool_use\"}}\n\n"
                "event: message_stop\ndata: {}\n\n", "text/event-stream");
        } else {
            response.set_content(openai_frame(
                R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"normal-failure","function":{"name":"FailingProbe","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})") +
                openai_frame("[DONE]"), "text/event-stream");
        }
    });
    class FailingProbe final : public loom::core::ITool {
        loom::core::ToolDefinition definition_{.name = "FailingProbe", .permission = loom::core::ToolPermission::ReadOnly};
    public:
        const loom::core::ToolDefinition& definition() const override { return definition_; }
        bool check_permission(const loom::core::ToolInput&) const override { return true; }
        loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
            return loom::core::ToolResult::error("controlled execution failure");
        }
    };
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<FailingProbe>());
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
    context->wire_api = protocol;
    config.api_context = context;
    loom::tools::AgentTool tool(config, 0, &registry);
    const auto result = tool.execute(agent_wire_input("normal-failed-tool"));
    ASSERT_TRUE(result && !result->is_error);
    const auto disk = runtime::load_native_agent_record("normal-failed-tool");
    ASSERT_TRUE(disk);
    EXPECT_EQ(disk->status, runtime::NativeAgentStatus::Completed);
    int saved = 0;
    for (const auto& entry : disk->sidechain_entries) {
        const auto parsed = json::parse(entry);
        ASSERT_TRUE(parsed);
        parsed->root().get("message").get("content").iter([&](json::JsonVal block) {
            if (block.get_string("tool_use_id") != "normal-failure") return;
            ++saved;
            EXPECT_TRUE(block.get("is_error").is_bool());
            EXPECT_TRUE(block.get("is_error").as_bool());
        });
    }
    EXPECT_EQ(saved, 1);
    const auto requests = server.bodies();
    ASSERT_EQ(requests.size(), 2u);
    const auto next = json::parse(requests.back());
    ASSERT_TRUE(next);
    int sent = 0;
    next->root().get("messages").iter([&](json::JsonVal message) {
        if (protocol == "messages") message.get("content").iter([&](json::JsonVal block) {
            if (block.get_string("type") != "tool_result" || block.get_string("tool_use_id") != "normal-failure") return;
            ++sent;
            EXPECT_TRUE(block.get("is_error").is_bool());
            EXPECT_TRUE(block.get("is_error").as_bool());
            EXPECT_EQ(block.get_string("content"), "controlled execution failure");
        });
        else if (message.get_string("role") == "tool" && message.get_string("tool_call_id") == "normal-failure") {
            ++sent;
            EXPECT_EQ(message.get_string("content"), "[tool_error] controlled execution failure");
        }
    });
    EXPECT_EQ(sent, 1);
}

namespace {
std::string instruction_system_text(const std::string& body) {
    const auto parsed = json::parse(body);
    if (!parsed) return {};
    if (const auto system = parsed->root().get("system"); system.is_str()) return std::string(system.as_str());
    std::string result;
    parsed->root().get("messages").iter([&](auto message) {
        if (message.get("role").is_str() && message.get("role").as_str() == "system" &&
            message.get("content").is_str()) result += message.get("content").as_str();
    });
    return result;
}
}

TEST(AgentWire, UserInstructionsReachBuiltInCustomAndDirectRegistryRequestsAcrossWorkingDirectories) {
    for (const auto protocol : {"messages", "openai"}) for (const auto route : {"built-in", "custom", "direct"}) {
        SCOPED_TRACE(std::format("{} {}", protocol, route));
        AgentWireSandbox sandbox;
        const auto home = sandbox.path / "home";
        const auto cwd = sandbox.path / (std::string("outside-home-") + route);
        std::filesystem::create_directories(home / ".loom");
        std::filesystem::create_directories(home / ".agents");
        std::filesystem::create_directories(cwd);
        { std::ofstream(home / ".agents" / "AGENTS.md") << "NATIVE_GLOBAL_RULE_942"; }
        AgentWireEnv isolated_home("HOME", home.string());
        AgentWireEnv no_override("LOOM_CONFIG_DIR", "");
        AgentWireEnv definitions("LOOM_AGENTS_JSON",
            R"({"global-custom-role":{"description":"Test custom role","prompt":"CUSTOM_ROLE_RULE_731"}})");
        AgentWireServer server([protocol](const auto&, auto& response, std::size_t) {
            response.set_content(std::string_view(protocol) == "messages" ? messages_text("global rules complete") :
                openai_text("global rules complete"), "text/event-stream");
        });
        auto context = std::make_shared<loom::core::ApiExecutionContext>(*agent_wire_context(server));
        context->wire_api = protocol;
        loom::core::ToolRegistry registry;
        auto input = loom::core::ToolInput::from_json(std::format(
            R"({{"prompt":"Complete the controlled request","agent_id":"global-rules-probe","cwd":"{}","subagent_type":"{}"}})",
            cwd.string(), std::string_view(route) == "custom" ? "global-custom-role" : "general-purpose"));
        input.api_context = context;
        input.parent_model = "wire-model";
        std::optional<loom::core::ToolResult> result;
        if (std::string_view(route) == "direct") {
            // This is the direct CLI's native registry path, with no parent
            // QueryEngine or inherited system prompt to supply global rules.
            loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
                .default_model = "wire-model", .api_context = context});
            auto executed = registry.execute("Agent", input);
            ASSERT_TRUE(executed);
            result = std::move(*executed);
        } else {
            loom::tools::AgentConfig config;
            config.default_model = "wire-model";
            config.api_context = context;
            loom::tools::AgentTool tool(config, 0, &registry);
            auto executed = tool.execute(input);
            ASSERT_TRUE(executed);
            result = std::move(*executed);
        }
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        const auto bodies = server.bodies();
        ASSERT_EQ(bodies.size(), 1u);
        const auto system = instruction_system_text(bodies.front());
        const auto at = system.find("NATIVE_GLOBAL_RULE_942");
        ASSERT_NE(at, std::string::npos);
        EXPECT_EQ(system.find("NATIVE_GLOBAL_RULE_942", at + 1), std::string::npos);
        EXPECT_NE(system.find("<context name=\"UserMemory\">"), std::string::npos);
        if (std::string_view(route) == "custom") EXPECT_NE(system.find("CUSTOM_ROLE_RULE_731"), std::string::npos);
    }
}

TEST(AgentWire, CompleteParentPromptPreservesOverrideWithoutDuplicatingInheritedUserInstructions) {
    for (const bool already_inherited : {false, true}) {
        AgentWireSandbox sandbox;
        AgentWireEnv isolated_home("HOME", sandbox.path.string());
        AgentWireEnv definitions("LOOM_AGENTS_JSON", "{}");
        const auto config_dir = sandbox.path / "isolated-config";
        std::filesystem::create_directories(config_dir);
        { std::ofstream(config_dir / "LOOM.md") << "INHERITED_GLOBAL_RULE_845"; }
        AgentWireEnv isolated_config("LOOM_CONFIG_DIR", config_dir.string());
        AgentWireServer server([](const auto&, auto& response, std::size_t) {
            response.set_content(openai_text("complete inherited prompt"), "text/event-stream");
        });
        const std::string complete_prompt = already_inherited
            ? "Complete parent rules: INHERITED_GLOBAL_RULE_845" : "Complete replacement excludes file instructions";
        json::JsonMutDoc doc;
        auto root = doc.object();
        root.add("prompt", doc.string("Finish the controlled override request"));
        root.add("agent_id", doc.string("global-override-probe"));
        root.add("parentSystemPrompt", doc.string(complete_prompt));
        doc.set_root(root);
        auto input = loom::core::ToolInput::from_json(doc.to_string());
        input.api_context = agent_wire_context(server);
        loom::core::ToolRegistry registry;
        loom::tools::AgentConfig config;
        config.default_model = "wire-model";
        loom::tools::AgentTool tool(config, 0, &registry);
        const auto result = tool.execute(input);
        ASSERT_TRUE(result);
        ASSERT_FALSE(result->is_error) << result_text(*result);
        const auto bodies = server.bodies();
        ASSERT_EQ(bodies.size(), 1u);
        EXPECT_EQ(instruction_system_text(bodies.front()), complete_prompt);
    }
}

TEST(AgentWire, ImplicitForkPreservesOneGlobalInstructionCopyThroughRealNestedRequests) {
    AgentWireSandbox sandbox;
    AgentWireEnv isolated_home("HOME", sandbox.path.string());
    AgentWireEnv definitions("LOOM_AGENTS_JSON", "{}");
    const auto config_dir = sandbox.path / "isolated-config";
    std::filesystem::create_directories(config_dir);
    { std::ofstream(config_dir / "LOOM.md") << "FORK_GLOBAL_RULE_348"; }
    AgentWireEnv isolated_config("LOOM_CONFIG_DIR", config_dir.string());
    AgentWireEnv nested("LOOM_ENABLE_NESTED_AGENTS", "1");
    AgentWireServer server([](const auto&, auto& response, std::size_t index) {
        if (index == 0) {
            response.set_content(openai_frame(R"({"choices":[{"delta":{"tool_calls":[{"index":0,"id":"global-fork-call","type":"function","function":{"name":"Agent","arguments":"{\"prompt\":\"Complete the inherited request\",\"agent_id\":\"global-fork-child\"}"}}]},"finish_reason":"tool_calls"}]})") + openai_frame("[DONE]"), "text/event-stream");
        } else response.set_content(openai_text("global fork complete"), "text/event-stream");
    });
    loom::core::ToolRegistry registry;
    auto allow = [](std::string_view, std::string_view, std::string_view) { return loom::tools::AgentLivePermissionCheck{}; };
    registry.register_tool(loom::tools::make_agent_tool({}, 0, &registry, allow));
    loom::tools::AgentConfig config;
    config.default_model = "wire-model";
    config.api_context = agent_wire_context(server);
    loom::tools::AgentTool parent(config, 0, &registry, allow);
    const auto result = parent.execute(agent_wire_input("global-fork-parent"));
    ASSERT_TRUE(result);
    ASSERT_FALSE(result->is_error) << result_text(*result);
    runtime::native_agent_store().wait_for_background_agents();
    const auto child = runtime::native_agent_store().get("global-fork-child");
    ASSERT_TRUE(child);
    EXPECT_EQ(child->status, runtime::NativeAgentStatus::Completed);
    const auto bodies = server.bodies();
    ASSERT_EQ(bodies.size(), 3u);
    const auto original_system = instruction_system_text(bodies.front());
    ASSERT_NE(original_system.find("FORK_GLOBAL_RULE_348"), std::string::npos);
    for (const auto& body : bodies) {
        const auto system = instruction_system_text(body);
        EXPECT_EQ(system, original_system);
        const auto at = system.find("FORK_GLOBAL_RULE_348");
        ASSERT_NE(at, std::string::npos);
        EXPECT_EQ(system.find("FORK_GLOBAL_RULE_348", at + 1), std::string::npos);
    }
}
