/// Subagent protocol integration through real loopback HTTP transfers.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.orchestration.agent;
import loom.tools.agent_runtime;
import loom.tools.tool;
import loom.services.api.streaming;
import loom.serdes.json;

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
        });
    }
    ASSERT_EQ(saved_results.size(), 3u);
    ASSERT_TRUE(saved_results.contains("cancel_batch_completed"));
    ASSERT_TRUE(saved_results.contains("cancel_batch_interrupted"));
    ASSERT_TRUE(saved_results.contains("cancel_batch_unexecuted"));
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
    EXPECT_EQ(resumed_results, saved_results);
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
