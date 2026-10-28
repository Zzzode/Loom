/// @file test_sdk_harness.cpp
/// @brief First-consumer gate for loom.sdk.harness (RFC 0001 cc-sdk phase 3,
///        design §4.4). A direct in-tree test consumer that constructs a
///        Harness, runs a turn against a loopback HTTP server serving canned
///        Messages API responses (the WireBackend seam does not
///        reach the transport — §2.4 — so a loopback server is the transport
///        mock), and asserts the TurnResult / events / resume / permission
///        bridge. Tests are not in the graph_check.py module graph (it scans
///        src/ only), so this creates no upward edge.
#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

import std;

import loom.types.types;
import loom.types.message_origin_json;
import loom.query.assembly;
import loom.serdes.json;
import loom.tools.tool;             // ToolInput, ToolResult, ToolPermission
import loom.tools.runtime_registry; // make_runtime_tool (detail namespace)
import loom.hooks.tool_permissions; // PermissionContext, PermissionResponse
import loom.session.storage;        // append_message (resume seed)
import loom.sdk.harness;

namespace {

namespace fs = std::filesystem;

// RAII loopback: httplib::Server on 127.0.0.1 with an ephemeral port,
// serving a fixed list of response bodies (indexed by request count) for
// POST /v1/messages. The real engine transport, SSE path, retry, and tool
// loop run end-to-end against this with zero external network calls.
class LoopbackServer {
public:
    LoopbackServer(std::vector<std::string> bodies, std::string content_type,
                   std::optional<std::string> summary_body = {})
        : bodies_(std::move(bodies))
        , content_type_(std::move(content_type))
        , summary_body_(std::move(summary_body)) {
        svr_.Post("/v1/messages", [this](const httplib::Request& request, httplib::Response& res) {
            {
                std::lock_guard lock(requests_mutex_);
                requests_.push_back(request.body);
            }
            if (summary_body_ && request.body.find("CONTEXT CHECKPOINT") != std::string::npos) {
                res.set_content(*summary_body_, "application/json");
                return;
            }
            const auto idx = request_count_.fetch_add(1);
            const auto& body = bodies_[std::min(idx, bodies_.size() - 1)];
            res.set_content(body, content_type_);
        });
        port_ = svr_.bind_to_any_port("127.0.0.1");
        if (port_ < 0) return;
        std::atomic<bool> up{false};
        th_ = std::thread([this, &up] {
            up.store(true, std::memory_order_release);
            svr_.listen_after_bind();
        });
        while (!up.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // macOS localhost loopback sometimes needs extra time to enter the
        // accept loop (same settle as test_sse_mock's MockServerRAII).
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~LoopbackServer() {
        if (port_ >= 0) {
            svr_.stop();
            if (th_.joinable()) th_.join();
        }
    }

    LoopbackServer(const LoopbackServer&) = delete;
    LoopbackServer& operator=(const LoopbackServer&) = delete;

    [[nodiscard]] bool ready() const noexcept { return port_ >= 0; }

    [[nodiscard]] std::string base_url() const { return std::format("http://127.0.0.1:{}", port_); }

    [[nodiscard]] std::vector<std::string> requests() const {
        std::lock_guard lock(requests_mutex_);
        return requests_;
    }

private:
    httplib::Server svr_;
    std::thread th_;
    int port_ = -1;
    std::vector<std::string> bodies_;
    std::string content_type_;
    std::optional<std::string> summary_body_;
    mutable std::mutex requests_mutex_;
    std::vector<std::string> requests_;
    std::atomic<std::size_t> request_count_{0};
};

std::string make_harness_config(loom::sdk::HarnessConfig& config, const std::string& base_url) {
    config.model = "loom-test";
    config.base_url = base_url;
    config.api_key_provider = [] { return "sk-test"; };
    return base_url;
}

// Canned non-streaming Messages API response (one text block).
constexpr std::string_view kTextResponse =
    R"({"id":"msg_test","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"ok"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})";

// Canned SSE stream for the streaming case (message_start -> text deltas ->
// message_stop), matching the shape the engine's SSE decoder expects.
std::string make_sse_body() {
    std::string out;
    out += "event: message_start\n"
           "data: {\"message\":{\"id\":\"msg_1\",\"type\":\"message\","
           "\"role\":\"assistant\",\"model\":\"loom-test\","
           "\"content\":[],\"stop_reason\":null,"
           "\"usage\":{\"input_tokens\":1,\"output_tokens\":0}}}\n\n";
    out += "event: content_block_start\n"
           "data: {\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n";
    out += "event: content_block_delta\n"
           "data: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n";
    out += "event: content_block_stop\n"
           "data: {\"index\":0}\n\n";
    out += "event: message_delta\n"
           "data: {\"delta\":{\"stop_reason\":\"end_turn\"},"
           "\"usage\":{\"output_tokens\":1}}\n\n";
    out += "event: message_stop\n"
           "data: {\"type\":\"message_stop\"}\n\n";
    return out;
}

// Canned tool_use response (first API call) + final text response (second).
constexpr std::string_view kToolUseResponse =
    R"({"id":"msg_1","type":"message","role":"assistant","model":"loom-test","content":[{"type":"tool_use","id":"toolu_1","name":"mock_perm_tool","input":{}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})";
constexpr std::string_view kFinalTextResponse =
    R"({"id":"msg_2","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"done"}],"stop_reason":"end_turn","usage":{"input_tokens":2,"output_tokens":1}})";

// ConfigManager reads process configuration at construction. Keep these public
// SDK tests independent of the developer's settings, project and output env.
class ModelBudgetSettings {
public:
    explicit ModelBudgetSettings(std::string_view endpoint)
        : previous_cwd_(fs::current_path())
        , root_(fs::temp_directory_path() /
                ("loom_sdk_model_budget_" + std::to_string(::getpid()) + "_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(root_ / "config");
        fs::create_directories(root_ / "work");
        for (const char* name : {"LOOM_CONFIG_DIR", "LOOM_MAX_TOKENS"}) {
            const auto* value = std::getenv(name);
            environment_.emplace_back(name,
                                      value ? std::optional<std::string>{value} : std::nullopt);
            unsetenv(name);
        }
        setenv("LOOM_CONFIG_DIR", (root_ / "config").string().c_str(), 1);
        fs::current_path(root_ / "work");
        loom::utils::json::JsonMutDoc doc;
        auto settings = doc.object();
        settings.add("contextWindowSize", doc.number(std::int64_t{200000}));
        settings.add("maxOutputTokens", doc.number(std::int64_t{64000}));
        settings.add("extendedThinking", doc.boolean(false));
        auto profiles = doc.array();
        for (const bool large : {true, false}) {
            auto profile = doc.object();
            profile.add("model", doc.string(large ? "sdk-window-large" : "sdk-window-small"));
            profile.add("contextWindowSize", doc.number(std::int64_t{large ? 1000000 : 256000}));
            profile.add("maxOutputTokens", doc.number(std::int64_t{large ? 64000 : 8192}));
            profile.add("baseUrl", doc.string(endpoint));
            profile.add("wireApi", doc.string("messages"));
            profiles.append(profile);
        }
        settings.add("modelContextLimits", profiles);
        doc.set_root(settings);
        std::ofstream output(root_ / "config" / "settings.json");
        output << doc.to_string();
    }

    ~ModelBudgetSettings() {
        std::error_code error;
        fs::current_path(previous_cwd_, error);
        for (const auto& [name, value] : environment_) {
            if (value)
                setenv(name.c_str(), value->c_str(), 1);
            else
                unsetenv(name.c_str());
        }
        fs::remove_all(root_, error);
    }

    [[nodiscard]] fs::path sessions_dir() const { return root_ / "sessions"; }

private:
    fs::path previous_cwd_;
    fs::path root_;
    std::vector<std::pair<std::string, std::optional<std::string>>> environment_;
};

std::string model_budget_checkpoint_response() {
    loom::utils::json::JsonMutDoc doc;
    auto response = doc.object();
    response.add("id", doc.string("budget-checkpoint"));
    response.add("type", doc.string("message"));
    response.add("role", doc.string("assistant"));
    response.add("model", doc.string("sdk-window-small"));
    response.add("stop_reason", doc.string("end_turn"));
    auto content = doc.array();
    auto block = doc.object();
    block.add("type", doc.string("text"));
    block.add(
        "text",
        doc.string(
            "## Goal\nContinue the SDK budget test.\n"
            "## Constraints\nKeep the current request.\n## Decisions\nUse the selected model.\n"
            "## Progress\nLarge history accepted.\n## Next Steps\nAnswer the next request.\n"
            "## References\nSDK model budget profiles.\n"));
    content.append(block);
    response.add("content", content);
    auto usage = doc.object();
    usage.add("input_tokens", doc.number(std::int64_t{1}));
    usage.add("output_tokens", doc.number(std::int64_t{1}));
    response.add("usage", usage);
    doc.set_root(response);
    return doc.to_string();
}

void exercise_sdk_model_budget_switch(bool streaming) {
    LoopbackServer server{{streaming ? make_sse_body() : std::string(kTextResponse)},
                          streaming ? "text/event-stream" : "application/json",
                          model_budget_checkpoint_response()};
    ASSERT_TRUE(server.ready());
    ModelBudgetSettings settings(server.base_url());
    loom::sdk::HarnessConfig config;
    make_harness_config(config, server.base_url());
    config.model = "sdk-window-small";
    config.wire_api = "messages";
    config.cwd = fs::current_path();
    config.sessions_dir = settings.sessions_dir();
    loom::sdk::Harness harness(std::move(config));
    const auto turn = [&](std::string model, std::string prompt) {
        loom::sdk::TurnOptions options;
        options.model = std::move(model);
        options.prompt = std::move(prompt);
        if (!streaming) {
            const auto result = harness.run(options);
            if (!result) {
                ADD_FAILURE() << result.error().format();
                return false;
            }
            return result->success;
        }
        bool ended = false;
        std::string errors;
        harness.stream(options, [&](const loom::core::StreamEvent& event) {
            if (std::holds_alternative<loom::core::StreamEnd>(event)) ended = true;
            if (const auto* error = std::get_if<loom::core::StreamError>(&event)) {
                errors += error->message;
            }
        });
        EXPECT_TRUE(errors.empty()) << errors;
        return ended && errors.empty();
    };
    // About 350K locally estimated tokens: above the actual 256K budget,
    // comfortably inside 1M. A small-window alias cannot pass this turn.
    const std::string large_history(1400000, 'h');
    ASSERT_TRUE(turn("sdk-window-large", large_history));
    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u) << "The 1M override must accept history without a summary";
    auto first = loom::utils::json::parse(requests.front());
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->root().get_string("model"), "sdk-window-large");
    EXPECT_EQ(first->root().get_int("max_tokens"), 64000);

    ASSERT_TRUE(turn("sdk-window-small", "Continue with the smaller model."));
    requests = server.requests();
    ASSERT_GT(requests.size(), 2u)
        << "The 256K override must summarize before its business request";
    ASSERT_LE(requests.size(), 18u) << "Compaction keeps the shared 16-request bound";
    for (std::size_t index = 1; index + 1 < requests.size(); ++index) {
        EXPECT_NE(requests[index].find("CONTEXT CHECKPOINT"), std::string::npos);
        auto summary = loom::utils::json::parse(requests[index]);
        ASSERT_TRUE(summary.has_value());
        EXPECT_EQ(summary->root().get_string("model"), "sdk-window-small");
        EXPECT_GT(summary->root().get_int("max_tokens"), 0);
        EXPECT_LE(summary->root().get_int("max_tokens"), 8192);
        EXPECT_FALSE(summary->root().get("stream").as_bool());
    }
    auto small = loom::utils::json::parse(requests.back());
    ASSERT_TRUE(small.has_value());
    EXPECT_EQ(small->root().get_string("model"), "sdk-window-small");
    EXPECT_EQ(small->root().get_int("max_tokens"), 8192);
    EXPECT_NE(requests.back().find("[Context checkpoint]"), std::string::npos);
    EXPECT_EQ(requests.back().find("CONTEXT CHECKPOINT"), std::string::npos);
    EXPECT_EQ(small->root().get("stream").as_bool(), streaming);

    const auto after_small = requests.size();
    ASSERT_TRUE(turn("sdk-window-large", large_history));
    requests = server.requests();
    ASSERT_EQ(requests.size(), after_small + 1) << "Switching back must restore the 1M window";
    auto restored = loom::utils::json::parse(requests.back());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->root().get_string("model"), "sdk-window-large");
    EXPECT_EQ(restored->root().get_int("max_tokens"), 64000)
        << "The smaller model's effective cap must not overwrite requested output";
    EXPECT_EQ(restored->root().get("stream").as_bool(), streaming);
}

}  // namespace

TEST(SdkHarness, RunModelOverrideRecomputesMillionAnd256KBudgetsAndRestoresOutput) {
    exercise_sdk_model_budget_switch(false);
}

TEST(SdkHarness, StreamModelOverrideRecomputesMillionAnd256KBudgetsAndRestoresOutput) {
    exercise_sdk_model_budget_switch(true);
}

// §4.4: run() against a loopback HTTP server serving a canned Messages API
// response — exercising the real transport with zero external calls.
TEST(SdkHarness, RunAgainstLoopback) {
    LoopbackServer server{std::vector<std::string>{std::string(kTextResponse)}, "application/json"};
    ASSERT_TRUE(server.ready());

    loom::sdk::HarnessConfig config;
    make_harness_config(config, server.base_url());
    loom::sdk::Harness harness(std::move(config));

    loom::sdk::TurnOptions options;
    options.prompt = "Hello";
    auto result = harness.run(options);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    EXPECT_EQ(result->usage.input_tokens, 1u);
    EXPECT_EQ(result->usage.output_tokens, 1u);
    EXPECT_EQ(result->tool_rounds, 0u);
    EXPECT_TRUE(result->success);

    // The assistant message carries the canned text block.
    bool found_text = false;
    for (const auto& block : result->message.content) {
        if (const auto* tb = std::get_if<loom::core::TextBlock>(&block)) {
            if (tb->text == "ok") found_text = true;
        }
    }
    EXPECT_TRUE(found_text);
    EXPECT_FALSE(harness.session_id().empty());
}

// §4.4: abort() before run() makes run() return an error once ("Query
// interrupted"), then the flag clears so the next turn proceeds. This works
// because the harness checks its own abort_requested_ flag at run() entry
// (the engine's aborted_ is auto-reset at query() entry).
TEST(SdkHarness, AbortBeforeRunReturnsErrorOnce) {
    // No server needed: the abort check is at run() entry, before any HTTP.
    loom::sdk::HarnessConfig config;
    config.model = "loom-test";
    config.api_key_provider = [] { return "sk-test"; };
    // Point at a closed local port so the second run()'s transport failure
    // is deterministic and local — NOT a real POST to an external
    // endpoint (which on a networked machine would be an external
    // call, violating §4.4's zero-external-calls gate).
    config.base_url = "http://127.0.0.1:1";
    loom::sdk::Harness harness(std::move(config));

    harness.abort();
    loom::sdk::TurnOptions options;
    options.prompt = "Hello";
    auto aborted = harness.run(options);
    ASSERT_FALSE(aborted.has_value());
    EXPECT_NE(aborted.error().message.find("Query interrupted"), std::string::npos)
        << aborted.error().format();

    // The flag cleared: a subsequent run() proceeds past the abort check
    // (and fails at the transport against the closed local port — proving
    // the abort did not poison the harness, with no external call).
    auto next = harness.run(options);
    ASSERT_FALSE(next.has_value());
    EXPECT_EQ(next.error().message.find("Query interrupted"), std::string::npos)
        << "a second run must not re-report the pre-run abort";
}

// §4.4: stream() delivers StreamEvents to the EventSink via the loopback SSE
// response.
TEST(SdkHarness, StreamDeliversEvents) {
    LoopbackServer server{std::vector<std::string>{make_sse_body()}, "text/event-stream"};
    ASSERT_TRUE(server.ready());

    loom::sdk::HarnessConfig config;
    make_harness_config(config, server.base_url());
    loom::sdk::Harness harness(std::move(config));

    std::atomic<int> text_deltas{0};
    std::atomic<bool> saw_stream_start{false};
    std::atomic<bool> saw_stream_end{false};
    std::string accumulated_text;
    std::mutex text_mutex;

    loom::sdk::TurnOptions options;
    options.prompt = "Hello";
    harness.stream(options, [&](const loom::core::StreamEvent& ev) {
        std::visit(
            [&](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, loom::core::StreamStart>) {
                    saw_stream_start.store(true);
                } else if constexpr (std::is_same_v<T, loom::core::ContentBlockDelta>) {
                    text_deltas.fetch_add(1);
                    std::lock_guard lock(text_mutex);
                    accumulated_text += e.delta_text;
                } else if constexpr (std::is_same_v<T, loom::core::StreamEnd>) {
                    saw_stream_end.store(true);
                }
            },
            ev);
    });

    EXPECT_TRUE(saw_stream_start.load());
    EXPECT_GE(text_deltas.load(), 1);
    EXPECT_TRUE(saw_stream_end.load());
    EXPECT_EQ(accumulated_text, "Hello");
}

// §4.4: resume(session_id) with sessions_dir restores a prior conversation
// (load_messages -> parse_session_message_value -> restore_conversation).
TEST(SdkHarness, ResumeRestoresConversation) {
    auto sessions_dir =
        fs::temp_directory_path() / ("loom_harness_resume_" + std::to_string(::getpid()));
    fs::create_directories(sessions_dir);
    const std::string session_id = "resume-test-session";

    // Seed a messages.jsonl via loom.session.storage (the same writer the
    // engine's session persistence uses).
    ASSERT_TRUE(loom::session::append_message(sessions_dir, session_id,
                                              R"({"id":"msg_1","role":"user","content":"Hello"})"));
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id,
        R"({"id":"msg_2","role":"assistant","content":"Hi there","model":"loom-test"})"));

    loom::sdk::HarnessConfig config;
    config.model = "loom-test";
    config.api_key_provider = [] { return "sk-test"; };
    config.sessions_dir = sessions_dir;
    loom::sdk::Harness harness(std::move(config));

    auto resumed = harness.resume(session_id);
    ASSERT_TRUE(resumed.has_value()) << resumed.error().format();

    auto conv = harness.conversation();
    ASSERT_EQ(conv.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<loom::core::UserMessage>(conv[0]));
    EXPECT_TRUE(std::holds_alternative<loom::core::AssistantMessage>(conv[1]));

    fs::remove_all(sessions_dir);
}

// §4.4: resume restores multi-block messages (thinking + tool_use + text)
// and tool_result messages.  Regression test: the original
// parse_session_message_value only read content-as-string and silently
// dropped every multi-block message, so thinking/tool_use chains vanished
// on resume (the UI showed only plain text, no chain compression).
TEST(SdkHarness, ResumeRestoresMultiBlockMessages) {
    auto sessions_dir =
        fs::temp_directory_path() / ("loom_harness_resume_multi_" + std::to_string(::getpid()));
    fs::create_directories(sessions_dir);
    const std::string session_id = "resume-multi-block";

    // User message (plain string content).
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id, R"({"id":"msg_1","role":"user","content":"Do the thing"})"));

    // Assistant message with thinking + tool_use + text (array content).
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id,
        std::string(R"({"id":"msg_2","role":"assistant","content":[)") +
            R"({"type":"thinking","thinking":"I should use Bash","signature":"sig1"},)" +
            R"({"type":"tool_use","id":"tu_1","name":"Bash","input":{"command":"ls"}},)" +
            R"({"type":"text","text":"Running ls..."}])" + R"(,"model":"loom-test"})"));

    // Tool result message (role="user" with tool_result object).
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id,
        R"({"id":"msg_3","role":"user","content":[)"
        R"({"type":"tool_result","tool_use_id":"tu_1","is_error":false,)"
        R"("content":"file1.txt\nfile2.txt"}]})"));

    loom::sdk::HarnessConfig config;
    config.model = "loom-test";
    config.api_key_provider = [] { return "sk-test"; };
    config.sessions_dir = sessions_dir;
    loom::sdk::Harness harness(std::move(config));

    auto resumed = harness.resume(session_id);
    ASSERT_TRUE(resumed.has_value()) << resumed.error().format();

    auto conv = harness.conversation();
    ASSERT_EQ(conv.size(), 3u);

    // Message 0: user text.
    ASSERT_TRUE(std::holds_alternative<loom::core::UserMessage>(conv[0]));
    const auto& um = std::get<loom::core::UserMessage>(conv[0]);
    ASSERT_EQ(um.content.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<loom::core::TextBlock>(um.content[0]));

    // Message 1: assistant with thinking + tool_use + text.
    ASSERT_TRUE(std::holds_alternative<loom::core::AssistantMessage>(conv[1]));
    const auto& am = std::get<loom::core::AssistantMessage>(conv[1]);
    ASSERT_EQ(am.content.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<loom::core::ThinkingBlock>(am.content[0]));
    EXPECT_TRUE(std::holds_alternative<loom::core::ToolUseBlock>(am.content[1]));
    EXPECT_TRUE(std::holds_alternative<loom::core::TextBlock>(am.content[2]));
    // Verify thinking content survived.
    const auto& thk = std::get<loom::core::ThinkingBlock>(am.content[0]);
    EXPECT_EQ(thk.thinking, "I should use Bash");
    EXPECT_EQ(thk.signature, "sig1");
    // Verify tool_use fields survived.
    const auto& tu = std::get<loom::core::ToolUseBlock>(am.content[1]);
    EXPECT_EQ(tu.name, "Bash");
    EXPECT_EQ(tu.id.value, "tu_1");
    // Verify model survived.
    EXPECT_EQ(am.model, "loom-test");

    // Message 2: tool result.
    ASSERT_TRUE(std::holds_alternative<loom::core::ToolResultMessage>(conv[2]));
    const auto& trm = std::get<loom::core::ToolResultMessage>(conv[2]);
    EXPECT_EQ(trm.tool_use_id.value, "tu_1");
    EXPECT_FALSE(trm.is_error);
    ASSERT_EQ(trm.content.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::TextBlock>(trm.content[0]));
    EXPECT_EQ(std::get<loom::core::TextBlock>(trm.content[0]).text, "file1.txt\nfile2.txt");

    fs::remove_all(sessions_dir);
}

// permission. A mock permission-gated tool is registered via the
// register_extra_tools seam; the canned first response calls it, the
// callback allows it, and the second response completes the turn.
TEST(SdkHarness, PermissionCallbackInvoked) {
    LoopbackServer server{
        std::vector<std::string>{std::string(kToolUseResponse), std::string(kFinalTextResponse)},
        "application/json"};
    ASSERT_TRUE(server.ready());

    std::atomic<int> permission_asks{0};
    loom::sdk::HarnessConfig config;
    make_harness_config(config, server.base_url());
    config.permission_callback =
        [&](const loom::hooks::PermissionContext& ctx) -> loom::hooks::PermissionResponse {
        EXPECT_EQ(ctx.tool_name, "mock_perm_tool");
        permission_asks.fetch_add(1);
        return loom::hooks::PermissionResponse{.decision = loom::hooks::PermissionDecision::allow,
                                               .updated_input_json = {},
                                               .updated_permissions_json = {},
                                               .message = {}};
    };
    // Register a mock tool that requires permission (the hook's
    // can_use_response reaches the ask_user callback when no rule matches).
    config.register_extra_tools = [](loom::core::ToolRegistry& registry) {
        registry.register_tool(loom::tools::detail::make_runtime_tool(
            "mock_perm_tool", "A mock tool that requires permission",
            loom::core::ToolPermission::Write, {}, /* no properties */
            [](const loom::core::ToolInput&) -> loom::core::Result<loom::core::ToolResult> {
                return loom::core::ToolResult::success("mock result");
            }));
    };

    loom::sdk::Harness harness(std::move(config));

    loom::sdk::TurnOptions options;
    options.prompt = "Use the mock tool";
    auto result = harness.run(options);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    EXPECT_GE(permission_asks.load(), 1)
        << "the PermissionCallback must be invoked for the mock tool";
    EXPECT_EQ(result->tool_rounds, 1u);
}

TEST(SdkHarness, ResumePreservesStructuredOriginsAndLegacyXmlWithoutInference) {
    using namespace loom::core;
    const auto sessions_dir =
        fs::temp_directory_path() /
        ("loom_harness_origins_" + std::to_string(::getpid()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::string session_id = "origin-session";
    const std::string body = "<task_notification>same &amp; /private/path</task_notification>";
    const std::vector<UserMessageOrigin> origins{
        HumanInput{}, LegacyHistory{},
        TaskEvent{"sub-1", session_id, TaskEventKind::Subagent, TaskEventStatus::Completed,
                  "Review"},
        TaskEvent{"tester", session_id, TaskEventKind::TeamAgent, TaskEventStatus::Stopped,
                  "Test 中文"}};
    for (const auto& origin : origins) {
        loom::utils::json::JsonMutDoc doc;
        auto object = doc.object();
        object.add("role", doc.string("user"));
        object.add("content", doc.string(body));
        write_user_message_origin(origin, object, doc);
        doc.set_root(object);
        ASSERT_TRUE(loom::session::append_message(sessions_dir, session_id, doc.to_string()));
    }
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id,
        R"({"role":"user","content":"<task_notification>old XML</task_notification>"})"));
    loom::sdk::HarnessConfig config;
    config.model = "loom-test";
    config.api_key_provider = [] { return "test-key"; };
    config.sessions_dir = sessions_dir;
    loom::sdk::Harness harness(std::move(config));
    ASSERT_TRUE(harness.resume(session_id).has_value());
    const auto messages = harness.conversation();
    ASSERT_EQ(messages.size(), origins.size() + 1);
    for (std::size_t index = 0; index < origins.size(); ++index) {
        ASSERT_TRUE(std::holds_alternative<UserMessage>(messages[index]));
        const auto& user = std::get<UserMessage>(messages[index]);
        EXPECT_EQ(user.origin, origins[index]);
        ASSERT_EQ(user.content.size(), 1u);
        EXPECT_EQ(std::get<TextBlock>(user.content.front()).text, body);
    }
    ASSERT_TRUE(std::holds_alternative<UserMessage>(messages.back()));
    EXPECT_TRUE(
        std::holds_alternative<LegacyHistory>(std::get<UserMessage>(messages.back()).origin));
    fs::remove_all(sessions_dir);
}

TEST(SdkHarness, SessionSeedParserPreservesOriginForStringAndBlockContent) {
    using namespace loom::core;
    const TaskEvent event{"agent-1", "parent-1", TaskEventKind::TeamAgent, TaskEventStatus::Failed,
                          "Name & <literal>"};
    for (const bool array_content : {false, true}) {
        for (const UserMessageOrigin& origin :
             {UserMessageOrigin{HumanInput{}}, UserMessageOrigin{LegacyHistory{}},
              UserMessageOrigin{event}}) {
            loom::utils::json::JsonMutDoc doc;
            auto object = doc.object();
            object.add("role", doc.string("user"));
            if (array_content) {
                auto blocks = doc.array();
                auto block = doc.object();
                block.add("type", doc.string("text"));
                block.add("text", doc.string("unchanged XML &amp; /private/result"));
                blocks.append(block);
                object.add("content", blocks);
            } else {
                object.add("content", doc.string("unchanged XML &amp; /private/result"));
            }
            write_user_message_origin(origin, object, doc);
            doc.set_root(object);
            auto parsed = loom::utils::json::parse(doc.to_string());
            ASSERT_TRUE(parsed.has_value());
            auto message = loom::query::parse_session_message_value(parsed->root(), 0);
            ASSERT_TRUE(message.has_value());
            ASSERT_TRUE(std::holds_alternative<UserMessage>(*message));
            const auto& user = std::get<UserMessage>(*message);
            EXPECT_EQ(user.origin, origin);
            ASSERT_EQ(user.content.size(), 1u);
            EXPECT_EQ(std::get<TextBlock>(user.content.front()).text,
                      "unchanged XML &amp; /private/result");
        }
    }
    for (const auto metadata : {R"("origin":null)", R"("origin":"unknown")",
                                R"("origin":"task_event","task_event":{})"}) {
        auto parsed = loom::utils::json::parse(
            std::string{R"({"role":"user","content":"keep raw XML",)"} + metadata + "}");
        ASSERT_TRUE(parsed.has_value());
        auto message = loom::query::parse_session_message_value(parsed->root(), 0);
        ASSERT_TRUE(message.has_value());
        const auto& user = std::get<UserMessage>(*message);
        EXPECT_TRUE(std::holds_alternative<HumanInput>(user.origin));
        EXPECT_EQ(std::get<TextBlock>(user.content.front()).text, "keep raw XML");
    }
}
