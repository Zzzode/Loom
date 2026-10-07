// Implementation unit for loom.query.fixture_serializer — StreamEvent /
// ContentBlock / Message → JSONL serialization in the RFC 0003 fixture
// format. The inverse of the parser in tests/streaming_replay.hpp.
module;

module loom.query.fixture_serializer;

import std;

import loom.types.types;
import loom.serdes.json;

namespace loom::core {

namespace {

namespace json = loom::utils::json;

// ── ContentBlock serialization ─────────────────────────────────────────

[[nodiscard]] json::JsonMutVal content_block_to_json_val(
    const ContentBlock& block, json::JsonMutDoc& doc) {
    auto obj = doc.object();

    if (const auto* text = std::get_if<TextBlock>(&block)) {
        obj.set("type", "text");
        obj.set("text", text->text);
    } else if (const auto* thinking = std::get_if<ThinkingBlock>(&block)) {
        obj.set("type", "thinking");
        obj.set("thinking", thinking->thinking);
        obj.set("signature", thinking->signature);
    } else if (const auto* tool_use = std::get_if<ToolUseBlock>(&block)) {
        obj.set("type", "tool_use");
        obj.set("id", tool_use->id.value);
        obj.set("name", tool_use->name);
        obj.set("input_json", tool_use->input_json);
    } else if (const auto* tool_result = std::get_if<ToolResultBlock>(&block)) {
        obj.set("type", "tool_result");
        obj.set("tool_use_id", tool_result->tool_use_id.value);
        obj.set("is_error", tool_result->is_error);
        if (const auto* str = std::get_if<std::string>(&tool_result->content)) {
            obj.set("content", *str);
        } else {
            const auto& items =
                std::get<std::vector<ToolResultContentItem>>(tool_result->content);
            auto arr = doc.array();
            for (const auto& item : items) {
                auto item_obj = doc.object();
                item_obj.set("type", item.type);
                if (!item.text.empty())
                    item_obj.set("text", item.text);
                if (!item.media_type.empty())
                    item_obj.set("media_type", item.media_type);
                if (!item.data.empty())
                    item_obj.set("data", item.data);
                arr.append(item_obj);
            }
            obj.add("content", arr);
        }
    } else if (const auto* image = std::get_if<ImageBlock>(&block)) {
        obj.set("type", "image");
        obj.set("media_type", image->media_type);
        obj.set("data", image->data);
        if (image->width)
            obj.set("width", static_cast<int>(*image->width));
        if (image->height)
            obj.set("height", static_cast<int>(*image->height));
    } else if (const auto* document = std::get_if<DocumentBlock>(&block)) {
        obj.set("type", "document");
        obj.set("media_type", document->media_type);
        obj.set("data", document->data);
    }

    return obj;
}

// ── Message serialization (for __commit__ lines) ───────────────────────

[[nodiscard]] json::JsonMutVal message_to_commit_json_val(
    const Message& msg, json::JsonMutDoc& doc) {
    auto msg_obj = doc.object();

    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;

        if constexpr (std::is_same_v<T, AssistantMessage>) {
            msg_obj.set("role", "assistant");
            if (m.model)
                msg_obj.set("model", *m.model);
            if (m.stop_reason)
                msg_obj.set("stop_reason", *m.stop_reason);
            auto content_arr = doc.array();
            for (const auto& block : m.content) {
                content_arr.append(content_block_to_json_val(block, doc));
            }
            msg_obj.add("content", content_arr);
        } else if constexpr (std::is_same_v<T, UserMessage>) {
            msg_obj.set("role", "user");
            auto content_arr = doc.array();
            for (const auto& block : m.content) {
                content_arr.append(content_block_to_json_val(block, doc));
            }
            msg_obj.add("content", content_arr);
        } else if constexpr (std::is_same_v<T, ToolResultMessage>) {
            msg_obj.set("role", "tool");
            msg_obj.set("tool_use_id", m.tool_use_id.value);
            msg_obj.set("tool_name", m.tool_name);
            msg_obj.set("is_error", m.is_error);
            auto content_arr = doc.array();
            for (const auto& block : m.content) {
                content_arr.append(content_block_to_json_val(block, doc));
            }
            msg_obj.add("content", content_arr);
        } else if constexpr (std::is_same_v<T, SystemMessage>) {
            // SystemMessage has no fixture representation; serialize as
            // a user message with empty content (the parser will accept
            // it, and the replay harness skips system commits).
            msg_obj.set("role", "user");
            msg_obj.add("content", doc.array());
        }
        // ToolUseMessage is not expected in commits — it's an internal
        // engine type. If it appears, serialize as assistant with empty
        // content.
        else if constexpr (std::is_same_v<T, ToolUseMessage>) {
            msg_obj.set("role", "assistant");
            msg_obj.add("content", doc.array());
        }
    }, msg);

    return msg_obj;
}

} // anonymous namespace

// ============================================================
// Public API
// ============================================================

[[nodiscard]] std::string content_block_to_json(const ContentBlock& block) {
    json::JsonMutDoc doc;
    auto val = content_block_to_json_val(block, doc);
    doc.set_root(val);
    return doc.to_string();
}

[[nodiscard]] std::string stream_event_to_json(const StreamEvent& ev) {
    json::JsonMutDoc doc;
    auto obj = doc.object();

    std::visit([&](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, StreamStart>) {
            obj.set("type", "stream_start");
            obj.set("message_id", e.message_id.value);
            obj.set("model", e.model);
        } else if constexpr (std::is_same_v<T, ContentBlockStart>) {
            obj.set("type", "content_block_start");
            obj.set("index", static_cast<int>(e.index));
            obj.add("block", content_block_to_json_val(e.block, doc));
        } else if constexpr (std::is_same_v<T, ContentBlockDelta>) {
            obj.set("type", "content_block_delta");
            obj.set("index", static_cast<int>(e.index));
            obj.set("delta_text", e.delta_text);
        } else if constexpr (std::is_same_v<T, ContentBlockStop>) {
            obj.set("type", "content_block_stop");
            obj.set("index", static_cast<int>(e.index));
        } else if constexpr (std::is_same_v<T, ToolExecutionStart>) {
            obj.set("type", "tool_execution_start");
            obj.set("tool_use_id", e.tool_use_id);
            obj.set("tool_name", e.tool_name);
            obj.set("input_json", e.input_json);
        } else if constexpr (std::is_same_v<T, ToolExecutionProgress>) {
            obj.set("type", "tool_execution_progress");
            obj.set("tool_use_id", e.tool_use_id);
            obj.set("partial_result", e.partial_result);
        } else if constexpr (std::is_same_v<T, ToolExecutionEnd>) {
            obj.set("type", "tool_execution_end");
            obj.set("tool_use_id", e.tool_use_id);
            obj.set("result", e.result);
            obj.set("is_error", e.is_error);
        } else if constexpr (std::is_same_v<T, StreamEnd>) {
            obj.set("type", "stream_end");
            if (e.stop_reason)
                obj.set("stop_reason", *e.stop_reason);
            auto usage_obj = doc.object();
            usage_obj.set("input_tokens", static_cast<int>(e.usage.input_tokens));
            usage_obj.set("output_tokens", static_cast<int>(e.usage.output_tokens));
            usage_obj.set("cache_creation_tokens",
                          static_cast<int>(e.usage.cache_creation_tokens));
            usage_obj.set("cache_read_tokens",
                          static_cast<int>(e.usage.cache_read_tokens));
            obj.add("usage", usage_obj);
        } else if constexpr (std::is_same_v<T, StreamError>) {
            obj.set("type", "stream_error");
            obj.set("error_type", e.error_type);
            obj.set("message", e.message);
        }
    }, ev);

    doc.set_root(obj);
    return doc.to_string();
}

[[nodiscard]] std::string commit_to_json(const Message& msg) {
    json::JsonMutDoc doc;
    auto obj = doc.object();
    obj.set("type", "__commit__");
    obj.add("message", message_to_commit_json_val(msg, doc));
    doc.set_root(obj);
    return doc.to_string();
}

[[nodiscard]] std::string end_query_to_json() {
    return R"({"type":"__end_query__"})";
}

[[nodiscard]] std::string checkpoint_to_json(std::string_view name) {
    json::JsonMutDoc doc;
    auto obj = doc.object();
    obj.set("type", "__checkpoint__");
    obj.set("name", name);
    doc.set_root(obj);
    return doc.to_string();
}

} // namespace loom::core
