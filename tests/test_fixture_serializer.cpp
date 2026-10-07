/// @file test_fixture_serializer.cpp
/// @brief RFC 0004 round-trip tests for the fixture serializer.
///
/// For each StreamEvent / ContentBlock / Message variant, serializes to
/// JSONL, parses back with the streaming_replay.hpp parser, serializes
/// again, and asserts the two JSON strings are identical (lossless
/// round-trip). Also tests the __end_query__ and __checkpoint__
/// pseudo-events.
///
/// This guards against drift between the serializer (RFC 0004 recorder)
/// and the parser (RFC 0003 replay harness) — they are separate code
/// paths that must agree on the fixture format.

#include <gtest/gtest.h>

#include "streaming_replay.hpp"

import std;
import loom.query.fixture_serializer;

using namespace loom::core;
namespace json = loom::utils::json;

namespace {

// ── Round-trip helpers ────────────────────────────────────────────────

/// Serialize a StreamEvent → parse → serialize again. Returns the second
/// serialization. If parsing fails, returns the first serialization (which
/// will cause the EXPECT_EQ to fail with a diff).
[[nodiscard]] std::string round_trip_event(const StreamEvent& ev) {
    const std::string s1 = stream_event_to_json(ev);
    auto doc = json::parse(s1);
    if (!doc) return s1;
    const auto root = doc->root();
    const auto type = root.get_string("type");
    auto ev2 = loom::testing::detail::parse_stream_event(root, type, 0);
    return stream_event_to_json(ev2);
}

/// Serialize a ContentBlock → parse → serialize again.
[[nodiscard]] std::string round_trip_block(const ContentBlock& block) {
    const std::string s1 = content_block_to_json(block);
    auto doc = json::parse(s1);
    if (!doc) return s1;
    auto block2 = loom::testing::detail::parse_content_block(doc->root(), 0);
    return content_block_to_json(block2);
}

/// Serialize a Message commit → parse → serialize again.
[[nodiscard]] std::string round_trip_commit(const Message& msg) {
    const std::string s1 = commit_to_json(msg);
    auto doc = json::parse(s1);
    if (!doc) return s1;
    auto step = loom::testing::detail::parse_step(doc->root(), 0);
    if (!step.message) return s1;
    return commit_to_json(*step.message);
}

// ── Test value factories ──────────────────────────────────────────────

[[nodiscard]] StreamStart make_stream_start() {
    return StreamStart{
        .message_id = MessageId{.value = "msg_001"},
        .model = "claude-fable-5",
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_text() {
    return ContentBlockStart{
        .index = 0,
        .block = TextBlock{.text = "Hello, world!"},
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_thinking() {
    return ContentBlockStart{
        .index = 1,
        .block = ThinkingBlock{
            .thinking = "Let me think about this...",
            .signature = "sig_abc123",
        },
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_tool_use() {
    return ContentBlockStart{
        .index = 2,
        .block = ToolUseBlock{
            .id = ToolUseId{.value = "tu_001"},
            .name = "Bash",
            .input_json = R"({"command":"ls -la"})",
        },
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_tool_result_str() {
    return ContentBlockStart{
        .index = 3,
        .block = ToolResultBlock{
            .tool_use_id = ToolUseId{.value = "tu_001"},
            .content = std::string("file1.txt\nfile2.txt"),
            .is_error = false,
        },
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_tool_result_arr() {
    return ContentBlockStart{
        .index = 4,
        .block = ToolResultBlock{
            .tool_use_id = ToolUseId{.value = "tu_002"},
            .content = std::vector<ToolResultContentItem>{
                {.type = "text", .text = "partial output", .media_type = "", .data = ""},
                {.type = "image", .text = "", .media_type = "image/png", .data = "iVBOR="},
            },
            .is_error = true,
        },
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_image() {
    return ContentBlockStart{
        .index = 5,
        .block = ImageBlock{
            .media_type = "image/png",
            .data = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAAB",
            .width = 800,
            .height = 600,
            .size_bytes = std::nullopt,
            .file_name = std::nullopt,
            .source_path = std::nullopt,
            .source = ImageBlockSource::Unknown,
        },
    };
}

[[nodiscard]] ContentBlockStart make_content_block_start_document() {
    return ContentBlockStart{
        .index = 6,
        .block = DocumentBlock{
            .media_type = "application/pdf",
            .data = "JVBERi0xLjcKJYGBg==",
        },
    };
}

[[nodiscard]] StreamEnd make_stream_end() {
    return StreamEnd{
        .stop_reason = "end_turn",
        .usage = TokenUsage{
            .input_tokens = 150,
            .output_tokens = 42,
            .cache_creation_tokens = 10,
            .cache_read_tokens = 100,
        },
    };
}

[[nodiscard]] StreamEnd make_stream_end_no_stop_reason() {
    return StreamEnd{
        .stop_reason = std::nullopt,
        .usage = TokenUsage{
            .input_tokens = 10,
            .output_tokens = 5,
        },
    };
}

[[nodiscard]] AssistantMessage make_assistant_message() {
    AssistantMessage msg;
    msg.model = "claude-fable-5";
    msg.stop_reason = "end_turn";
    msg.content.push_back(TextBlock{.text = "The answer is 42."});
    msg.content.push_back(TextBlock{.text = " Here is why..."});
    return msg;
}

[[nodiscard]] UserMessage make_user_message() {
    UserMessage msg;
    msg.content.push_back(TextBlock{.text = "What is the meaning of life?"});
    return msg;
}

[[nodiscard]] ToolResultMessage make_tool_result_message() {
    ToolResultMessage msg;
    msg.tool_use_id = ToolUseId{.value = "tu_001"};
    msg.tool_name = "Bash";
    msg.is_error = false;
    msg.content.push_back(TextBlock{.text = "file1.txt\nfile2.txt"});
    return msg;
}

} // anonymous namespace

// ============================================================
// StreamEvent round-trip tests
// ============================================================

TEST(FixtureSerializer, StreamStartRoundTrip) {
    StreamEvent ev = make_stream_start();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartTextRoundTrip) {
    StreamEvent ev = make_content_block_start_text();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartThinkingRoundTrip) {
    StreamEvent ev = make_content_block_start_thinking();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartToolUseRoundTrip) {
    StreamEvent ev = make_content_block_start_tool_use();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartToolResultStrRoundTrip) {
    StreamEvent ev = make_content_block_start_tool_result_str();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartToolResultArrRoundTrip) {
    StreamEvent ev = make_content_block_start_tool_result_arr();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartImageRoundTrip) {
    StreamEvent ev = make_content_block_start_image();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStartDocumentRoundTrip) {
    StreamEvent ev = make_content_block_start_document();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockDeltaRoundTrip) {
    StreamEvent ev = ContentBlockDelta{
        .index = 0,
        .delta_text = "incremental text",
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ContentBlockStopRoundTrip) {
    StreamEvent ev = ContentBlockStop{.index = 0};
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ToolExecutionStartRoundTrip) {
    StreamEvent ev = ToolExecutionStart{
        .tool_use_id = "tu_001",
        .tool_name = "Bash",
        .input_json = R"({"command":"ls"})",
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ToolExecutionProgressRoundTrip) {
    StreamEvent ev = ToolExecutionProgress{
        .tool_use_id = "tu_001",
        .partial_result = "file1.txt\n",
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ToolExecutionEndRoundTrip) {
    StreamEvent ev = ToolExecutionEnd{
        .tool_use_id = "tu_001",
        .result = "done",
        .is_error = false,
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, ToolExecutionEndErrorRoundTrip) {
    StreamEvent ev = ToolExecutionEnd{
        .tool_use_id = "tu_002",
        .result = "command not found",
        .is_error = true,
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, StreamEndRoundTrip) {
    StreamEvent ev = make_stream_end();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, StreamEndNoStopReasonRoundTrip) {
    StreamEvent ev = make_stream_end_no_stop_reason();
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

TEST(FixtureSerializer, StreamErrorRoundTrip) {
    StreamEvent ev = StreamError{
        .error_type = "rate_limit",
        .message = "Too many requests",
    };
    EXPECT_EQ(stream_event_to_json(ev), round_trip_event(ev));
}

// ============================================================
// ContentBlock direct round-trip tests
// ============================================================

TEST(FixtureSerializer, TextBlockRoundTrip) {
    ContentBlock b = TextBlock{.text = "plain text"};
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, TextBlockEmptyRoundTrip) {
    ContentBlock b = TextBlock{.text = ""};
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ThinkingBlockRoundTrip) {
    ContentBlock b = ThinkingBlock{
        .thinking = "reasoning",
        .signature = "sig",
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ToolUseBlockRoundTrip) {
    ContentBlock b = ToolUseBlock{
        .id = ToolUseId{.value = "tu_999"},
        .name = "Read",
        .input_json = R"({"path":"/tmp/file.txt"})",
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ToolResultBlockStringRoundTrip) {
    ContentBlock b = ToolResultBlock{
        .tool_use_id = ToolUseId{.value = "tu_999"},
        .content = std::string("simple result"),
        .is_error = false,
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ToolResultBlockArrayRoundTrip) {
    ContentBlock b = ToolResultBlock{
        .tool_use_id = ToolUseId{.value = "tu_999"},
        .content = std::vector<ToolResultContentItem>{
            {.type = "text", .text = "line 1", .media_type = "", .data = ""},
            {.type = "text", .text = "line 2", .media_type = "", .data = ""},
        },
        .is_error = false,
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ToolResultBlockErrorRoundTrip) {
    ContentBlock b = ToolResultBlock{
        .tool_use_id = ToolUseId{.value = "tu_999"},
        .content = std::string("permission denied"),
        .is_error = true,
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ImageBlockRoundTrip) {
    ContentBlock b = ImageBlock{
        .media_type = "image/jpeg",
        .data = "/9j/4AAQSkZJRg==",
        .width = 1024,
        .height = 768,
        .size_bytes = std::nullopt,
        .file_name = std::nullopt,
        .source_path = std::nullopt,
        .source = ImageBlockSource::Unknown,
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, ImageBlockNoDimensionsRoundTrip) {
    ContentBlock b = ImageBlock{
        .media_type = "image/png",
        .data = "iVBOR=",
        .width = std::nullopt,
        .height = std::nullopt,
        .size_bytes = std::nullopt,
        .file_name = std::nullopt,
        .source_path = std::nullopt,
        .source = ImageBlockSource::Unknown,
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

TEST(FixtureSerializer, DocumentBlockRoundTrip) {
    ContentBlock b = DocumentBlock{
        .media_type = "application/pdf",
        .data = "JVBERi0xLjcK",
    };
    EXPECT_EQ(content_block_to_json(b), round_trip_block(b));
}

// ============================================================
// Message commit round-trip tests
// ============================================================

TEST(FixtureSerializer, AssistantCommitRoundTrip) {
    Message msg = make_assistant_message();
    EXPECT_EQ(commit_to_json(msg), round_trip_commit(msg));
}

TEST(FixtureSerializer, UserCommitRoundTrip) {
    Message msg = make_user_message();
    EXPECT_EQ(commit_to_json(msg), round_trip_commit(msg));
}

TEST(FixtureSerializer, ToolResultCommitRoundTrip) {
    Message msg = make_tool_result_message();
    EXPECT_EQ(commit_to_json(msg), round_trip_commit(msg));
}

TEST(FixtureSerializer, AssistantCommitNoModelRoundTrip) {
    AssistantMessage m;
    m.content.push_back(TextBlock{.text = "no model here"});
    Message msg = m;
    EXPECT_EQ(commit_to_json(msg), round_trip_commit(msg));
}

TEST(FixtureSerializer, AssistantCommitMultiBlockRoundTrip) {
    AssistantMessage m;
    m.model = "claude-fable-5";
    m.stop_reason = "tool_use";
    m.content.push_back(TextBlock{.text = "Let me check that."});
    m.content.push_back(ToolUseBlock{
        .id = ToolUseId{.value = "tu_010"},
        .name = "Bash",
        .input_json = R"({"command":"pwd"})",
    });
    Message msg = m;
    EXPECT_EQ(commit_to_json(msg), round_trip_commit(msg));
}

// ============================================================
// Pseudo-event tests
// ============================================================

TEST(FixtureSerializer, EndQueryRoundTrip) {
    const std::string s = end_query_to_json();
    auto doc = json::parse(s);
    ASSERT_TRUE(doc.has_value());
    auto step = loom::testing::detail::parse_step(doc->root(), 0);
    EXPECT_EQ(step.kind, loom::testing::ReplayStepKind::EndQuery);
}

TEST(FixtureSerializer, CheckpointRoundTrip) {
    const std::string s = checkpoint_to_json("after_delta");
    auto doc = json::parse(s);
    ASSERT_TRUE(doc.has_value());
    auto step = loom::testing::detail::parse_step(doc->root(), 0);
    EXPECT_EQ(step.kind, loom::testing::ReplayStepKind::Checkpoint);
    EXPECT_EQ(step.checkpoint_name, "after_delta");
}

// ============================================================
// JSON validity smoke tests
// ============================================================

TEST(FixtureSerializer, AllEventsProduceValidJson) {
    const StreamEvent events[] = {
        make_stream_start(),
        make_content_block_start_text(),
        make_content_block_start_thinking(),
        make_content_block_start_tool_use(),
        make_content_block_start_tool_result_str(),
        make_content_block_start_tool_result_arr(),
        make_content_block_start_image(),
        make_content_block_start_document(),
        ContentBlockDelta{.index = 0, .delta_text = "x"},
        ContentBlockStop{.index = 0},
        ToolExecutionStart{.tool_use_id = "t", .tool_name = "T", .input_json = "{}"},
        ToolExecutionProgress{.tool_use_id = "t", .partial_result = "p"},
        ToolExecutionEnd{.tool_use_id = "t", .result = "r", .is_error = false},
        make_stream_end(),
        StreamError{.error_type = "e", .message = "m"},
    };
    for (const auto& ev : events) {
        const std::string s = stream_event_to_json(ev);
        auto doc = json::parse(s);
        EXPECT_TRUE(doc.has_value()) << "Invalid JSON: " << s;
    }
}
