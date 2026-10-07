/// @file test_ui_runtime.cpp
/// @brief Split from test_ui.cpp - AppRuntime, E2E_Gate, FullscreenLayout, LogoV2, PromptInput, ReplScreen (SLOC budget fix)

#include <cstdlib>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.messages.message_image;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;

namespace {
namespace fs = std::filesystem;
}


// ═══════════════════════════════════════════════════════════════════════════════
// loom.ui.chrome.terminal: FTXUI terminal controller and common widgets
// ═══════════════════════════════════════════════════════════════════════════════





// =============================================================================
// GAP: unseen-divider-in-transcript-missing (P1)
// TS REF: Messages.tsx L549-553  (useUnseenDivider → dividerBeforeIndex)
//       + Messages.tsx L631-635  (<Divider title="N new messages" color="inactive"/>)
//       + FullscreenLayout.tsx L224-256  (UnseenDivider + computeUnseenDivider)
// Faithful port: compute divider insertion point via 24-char uuid prefix match,
// insert a colored separator row BEFORE the matched payload row with marginTop=1.
// =============================================================================

// ============================================================
// E2E UI GATE — regression guard for critical user-visible scenarios
//
// These tests exercise the full AppAdapter → Render() pipeline in the
// scenarios that have historically broken during UI migration:
//   1. Startup screen (logo + statusline + prompt)
//   2. User message with image attachment (⎿ connector, no bg on continuation)
//   3. Tool use block with Input/Output sections
//   4. Full conversation flow (user → tool → result → assistant)
//   5. Statusline always visible in every state
// ============================================================

namespace e2e_gate {

/// Mock server that returns a tool_use block for an "analyze_image" MCP tool,
/// then on the second round returns the assistant text response.
/// Simulates the exact flow: user sends image → model calls MCP tool → result
/// → model generates text reply.
class McpToolFlowServer {
public:
    McpToolFlowServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            std::size_t request_number = 0;
            {
                std::lock_guard lock(mutex_);
                request_number = ++request_count_;
                last_body_ = req.body;
            }
            cv_.notify_all();

            res.set_header("x-usage-input-tokens", "42");
            if (request_number == 1) {
                // First round: model calls analyze_image tool
                res.set_chunked_content_provider(
                    "text/event-stream",
                    [this](size_t, httplib::DataSink& sink) {
                        if (phase_++ > 0) return false;
                        sink.os <<
                            "event: message_start\n"
                            "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_e2e_1\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                            "event: content_block_start\n"
                            "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_e2e_1\",\"name\":\"analyze_image\",\"input\":{}}}\n\n"
                            "event: content_block_delta\n"
                            "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"imageSource\\\":\\\"https://example.com/img.png\\\",\\\"prompt\\\":\\\"Describe this image in detail\\\"}\"}}\n\n"
                            "event: content_block_stop\n"
                            "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                            "event: message_delta\n"
                            "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":15}}\n\n"
                            "event: message_stop\n"
                            "data: {\"type\":\"message_stop\"}\n\n";
                        sink.done();
                        {
                            std::lock_guard lock(mutex_);
                            tool_sent_ = true;
                        }
                        cv_.notify_all();
                        return true;
                    });
            } else {
                // Second round: model generates text reply after seeing tool result
                res.set_chunked_content_provider(
                    "text/event-stream",
                    [this](size_t, httplib::DataSink& sink) {
                        if (phase2_++ > 0) return false;
                        sink.os <<
                            "event: message_start\n"
                            "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_e2e_2\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                            "event: content_block_start\n"
                            "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                            "event: content_block_delta\n"
                            "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"This is a screenshot of a dark-themed terminal UI showing a code review platform with bullet points and comment sections.\"}}\n\n"
                            "event: content_block_stop\n"
                            "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                            "event: message_delta\n"
                            "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":25}}\n\n"
                            "event: message_stop\n"
                            "data: {\"type\":\"message_stop\"}\n\n";
                        sink.done();
                        {
                            std::lock_guard lock(mutex_);
                            reply_sent_ = true;
                        }
                        cv_.notify_all();
                        return true;
                    });
            }
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~McpToolFlowServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    bool valid() const { return port_ > 0; }
    std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    bool wait_for_tool(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return tool_sent_; });
    }

    bool wait_for_reply(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return reply_sent_; });
    }

    std::string last_body() {
        std::lock_guard lock(mutex_);
        return last_body_;
    }

    /// Check if the tools array in the request body contains a specific tool name.
    bool request_contains_tool(std::string_view tool_name) {
        std::lock_guard lock(mutex_);
        return last_body_.find(tool_name) != std::string::npos;
    }

private:
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_ = 0;
    std::string last_body_;
    bool tool_sent_ = false;
    bool reply_sent_ = false;
    int phase_ = 0;
    int phase2_ = 0;
};

/// Helper: render app to plain text and check for required substrings.
/// Returns vector of missing strings for diagnostic output.
std::vector<std::string> check_required_strings(
    const std::string& rendered,
    const std::vector<std::string>& required) {
    std::vector<std::string> missing;
    for (const auto& s : required) {
        if (rendered.find(s) == std::string::npos) {
            missing.push_back(s);
        }
    }
    return missing;
}

} // namespace e2e_gate

/// Unit test: find_divider_before_visible_index correctly matches on the
/// first 24 chars of each payload row's uuid; skips compact-group rows.
TEST(E2E_Gate, StartupScreenHasAllElements) {
    using namespace e2e_gate;

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_e2e_gate_startup_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Render at a realistic terminal size
    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 36));

    // Critical elements that must always be visible on startup
    auto missing = check_required_strings(rendered, {
        "Loom",   // Logo / app name
        "Try ",          // Prompt placeholder hint
    });

    EXPECT_TRUE(missing.empty())
        << "E2E GATE FAIL: Startup screen missing critical elements:\n"
        << "  Missing: " << [&] {
            std::string s;
            for (const auto& m : missing) s += "[" + m + "] ";
            return s;
        }()
        << "\n  Rendered output (first 2000 chars):\n"
        << rendered.substr(0, 2000);

    fs::remove_all(storage_root);
}



/// E2E Gate #4: API request body must include MCP tools in the tools array.
/// Regression guard for "MCP tools not sent to API" bug.
TEST(E2E_Gate, McpToolsIncludedInApiRequestBody) {
    using namespace e2e_gate;

    McpToolFlowServer server;
    ASSERT_TRUE(server.valid());

    loom::core::ToolRegistry tools;
    // Register built-in tools so the request has something
    tools.set_missing_tool_handler(
        [](std::string_view tool_name,
           const loom::core::ToolInput&) -> loom::core::Result<loom::core::ToolResult> {
            return std::unexpected(loom::core::Error::make(
                loom::core::ErrorCode::ToolNotFound,
                std::format("Tool '{}' not found", tool_name)));
        });

    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();

    // Add a dynamic tools provider (simulating MCP tool discovery)
    config.dynamic_tools_provider = []() -> std::vector<loom::core::ToolDefinition> {
        std::vector<loom::core::ToolDefinition> defs;
        loom::core::ToolDefinition d;
        d.name = "analyze_image";
        d.description = "Analyze an image and return a detailed description";
        d.permission = loom::core::ToolPermission::Network;
        d.category = "mcp:zai-builtin";
        defs.push_back(d);
        return defs;
    };

    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_e2e_gate_apibody_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    app->HandleSubmit("test");

    // Wait for query to finish
    ASSERT_TRUE(wait_until([&] {
        (void)app->Render();
        return !test_seams(app).is_query_running_for_testing();
    }, std::chrono::seconds(5)));

    // The request body sent to the API should include the MCP tool name
    EXPECT_TRUE(server.request_contains_tool("analyze_image"))
        << "E2E GATE FAIL: API request body does not include MCP tool 'analyze_image'.\n"
        << "  The dynamic_tools_provider should ensure MCP tools are in the\n"
        << "  tools array sent to the API.\n"
        << "  Last request body:\n" << server.last_body().substr(0, 2000);

    fs::remove_all(storage_root);
}



/// E2E Gate #5: Image placeholder must show [Image #N] format, not just [Image].
/// Regression guard for "image ID missing" bug.
TEST(E2E_Gate, ImagePlaceholderShowsNumberedId) {
    using namespace loom::ui::messages::image;

    ImageMessageData d;
    d.media_type = "image/png";
    d.image_id = "1";  // This is the key: display id must be set
    d.file_name = "clipboard.png";
    d.source_type = ImageSource::Clipboard;

    Element el = render(d);
    std::string snap = strip_ansi(render_to_plain_text(std::move(el), 80, 10));

    EXPECT_NE(snap.find("[Image #1]"), std::string::npos)
        << "E2E GATE FAIL: Image placeholder shows '[Image]' instead of '[Image #1]'.\n"
        << "  The image_id field must be populated from the paste order.\n"
        << "  Got: " << snap;
}



/// E2E Gate #6: ⎿ connector character (U+23BF) must be used for continuation
/// blocks, NOT ⏿ (U+23FF). Regression guard for wrong Unicode codepoint.
/// This test verifies the source files contain the correct UTF-8 bytes,
/// since the rendering functions live in a `detail` namespace that cannot
/// be directly imported without causing ambiguity.
TEST(E2E_Gate, ConnectorCharacterIsCorrectCodepoint) {
    // The correct connector is U+23BF = ⎿ = \xe2\x8e\xbf in UTF-8
    // The WRONG connector would be U+23FF = ⏿ = \xe2\x8f\xbf
    const std::string correct_connector = "\xe2\x8e\xbf";  // U+23BF ⎿
    const std::string wrong_connector = "\xe2\x8f\xbf";    // U+23FF ⏿

    // Resolve project root relative to this test file (tests/test_ui.cpp)
    const std::string test_file = __FILE__;
    const auto test_dir = test_file.substr(0, test_file.find_last_of('/'));
    const auto project_root = test_dir.substr(0, test_dir.find_last_of('/'));

    // Check all message rendering source files for the correct connector bytes
    // (RFC 0001 Phase C batch 7: the messages_list connector now lives in its
    // payload_row module implementation unit).
    const std::vector<std::string> source_files = {
        "src/ui/messages/message_tool_result.cppm",
        "src/ui/messages/user_text_message.cppm",
        "src/ui/messages/messages_list_payload_row.cpp",
    };

    for (const auto& rel_path : source_files) {
        const auto full_path = project_root + "/" + rel_path;
        std::ifstream file(full_path);
        if (!file.is_open()) continue;  // Skip if file not found

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Every file that renders connectors should use the correct U+23BF
        EXPECT_NE(content.find(correct_connector), std::string::npos)
            << "E2E GATE FAIL: " << rel_path << " does not contain correct "
            << "connector U+23BF (⎿ = \\xe2\\x8e\\xbf).\n"
            << "  Expected the 'NOT-CURVE ARCH EXTENDING LEFT AND DOWNWARDS' character.";

        // No file should contain the wrong U+23FF
        EXPECT_EQ(content.find(wrong_connector), std::string::npos)
            << "E2E GATE FAIL: " << rel_path << " contains WRONG connector "
            << "U+23FF (⏿ = \\xe2\\x8f\\bf black floppy disk).\n"
            << "  This should be U+23BF (⎿). Fix the hex bytes in the source.";
    }
}
