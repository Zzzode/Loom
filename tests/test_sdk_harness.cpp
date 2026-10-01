/// @file test_sdk_harness.cpp
/// @brief First-consumer gate for loom.sdk.harness (RFC 0001 cc-sdk phase 3,
///        design §4.4). A direct in-tree test consumer that constructs a
///        Harness, runs a turn against a loopback HTTP server serving canned
///        Messages API responses (the WireBackend seam does not
///        reach the transport — §2.4 — so a loopback server is the transport
///        mock), and asserts the TurnResult / events / resume / permission
///        bridge. Tests are not in the graph_check.py module graph (it scans
///        src/ only), so this creates no upward edge.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>

import std;

import loom.types.types;
import loom.tools.tool;              // ToolInput, ToolResult, ToolPermission
import loom.tools.runtime_registry;  // make_runtime_tool (detail namespace)
import loom.hooks.tool_permissions;  // PermissionContext, PermissionResponse
import loom.session.storage;         // append_message (resume seed)
import loom.sdk.harness;

namespace {

namespace fs = std::filesystem;

// RAII loopback: httplib::Server on 127.0.0.1 with an ephemeral port,
// serving a fixed list of response bodies (indexed by request count) for
// POST /v1/messages. The real engine transport, SSE path, retry, and tool
// loop run end-to-end against this with zero external network calls.
class LoopbackServer {
public:
    LoopbackServer(std::vector<std::string> bodies, std::string content_type)
        : bodies_(std::move(bodies)), content_type_(std::move(content_type)) {
        svr_.Post("/v1/messages", [this](const httplib::Request&,
                                         httplib::Response& res) {
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
    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

private:
    httplib::Server svr_;
    std::thread th_;
    int port_ = -1;
    std::vector<std::string> bodies_;
    std::string content_type_;
    std::atomic<std::size_t> request_count_{0};
};

std::string make_harness_config(loom::sdk::HarnessConfig& config,
                                const std::string& base_url) {
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

} // namespace

// §4.4: run() against a loopback HTTP server serving a canned Messages API
// response — exercising the real transport with zero external calls.
TEST(SdkHarness, RunAgainstLoopback) {
    LoopbackServer server{std::vector<std::string>{std::string(kTextResponse)},
                          "application/json"};
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
    EXPECT_NE(aborted.error().message.find("Query interrupted"),
              std::string::npos)
        << aborted.error().format();

    // The flag cleared: a subsequent run() proceeds past the abort check
    // (and fails at the transport against the closed local port — proving
    // the abort did not poison the harness, with no external call).
    auto next = harness.run(options);
    ASSERT_FALSE(next.has_value());
    EXPECT_EQ(next.error().message.find("Query interrupted"),
              std::string::npos)
        << "a second run must not re-report the pre-run abort";
}

// §4.4: stream() delivers StreamEvents to the EventSink via the loopback SSE
// response.
TEST(SdkHarness, StreamDeliversEvents) {
    LoopbackServer server{std::vector<std::string>{make_sse_body()},
                          "text/event-stream"};
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
    auto sessions_dir = fs::temp_directory_path() /
        ("loom_harness_resume_" + std::to_string(::getpid()));
    fs::create_directories(sessions_dir);
    const std::string session_id = "resume-test-session";

    // Seed a messages.jsonl via loom.session.storage (the same writer the
    // engine's session persistence uses).
    ASSERT_TRUE(loom::session::append_message(
        sessions_dir, session_id,
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

// §4.4: the PermissionCallback is invoked for a tool that requires
// permission. A mock permission-gated tool is registered via the
// register_extra_tools seam; the canned first response calls it, the
// callback allows it, and the second response completes the turn.
TEST(SdkHarness, PermissionCallbackInvoked) {
    LoopbackServer server{
        std::vector<std::string>{std::string(kToolUseResponse),
                                 std::string(kFinalTextResponse)},
        "application/json"};
    ASSERT_TRUE(server.ready());

    std::atomic<int> permission_asks{0};
    loom::sdk::HarnessConfig config;
    make_harness_config(config, server.base_url());
    config.permission_callback =
        [&](const loom::hooks::PermissionContext& ctx)
            -> loom::hooks::PermissionResponse {
            EXPECT_EQ(ctx.tool_name, "mock_perm_tool");
            permission_asks.fetch_add(1);
            return loom::hooks::PermissionResponse{
                .decision = loom::hooks::PermissionDecision::allow,
                .updated_input_json = {},
                .updated_permissions_json = {},
                .message = {}};
        };
    // Register a mock tool that requires permission (the hook's
    // can_use_response reaches the ask_user callback when no rule matches).
    config.register_extra_tools = [](loom::core::ToolRegistry& registry) {
        registry.register_tool(loom::tools::detail::make_runtime_tool(
            "mock_perm_tool", "A mock tool that requires permission",
            loom::core::ToolPermission::Write,
            {}, /* no properties */
            [](const loom::core::ToolInput&)
                -> loom::core::Result<loom::core::ToolResult> {
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
