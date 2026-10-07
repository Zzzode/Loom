module;

export module loom.query.fixture_serializer;

import std;

import loom.types.types;
import loom.serdes.json;

export namespace loom::core {

// ============================================================
// Fixture serializer (RFC 0004)
//
// Serializes StreamEvent, ContentBlock, and Message values to JSONL
// lines in the RFC 0003 fixture format. This is the INVERSE of the
// parser in tests/streaming_replay.hpp — the round-trip test in
// tests/test_fixture_serializer.cpp guards against drift.
//
// The fixture format differs from the wire format produced by
// message_to_jsonl_():
//   - ToolResultMessage: role "tool" (not "user"), with top-level
//     tool_use_id/tool_name/is_error
//   - AssistantMessage: includes model and stop_reason fields
//   - ContentBlock: uses "type" discriminator (text/thinking/tool_use/
//     tool_result/image/document)
// ============================================================

/// Serialize a ContentBlock to a JSON object string.
[[nodiscard]] std::string content_block_to_json(const ContentBlock& block);

/// Serialize a StreamEvent to a JSONL line (no trailing newline).
[[nodiscard]] std::string stream_event_to_json(const StreamEvent& ev);

/// Serialize a Message to a __commit__ JSONL line (no trailing newline).
[[nodiscard]] std::string commit_to_json(const Message& msg);

/// Serialize an __end_query__ JSONL line.
[[nodiscard]] std::string end_query_to_json();

/// Serialize a __checkpoint__ JSONL line with the given name.
[[nodiscard]] std::string checkpoint_to_json(std::string_view name);

} // namespace loom::core
