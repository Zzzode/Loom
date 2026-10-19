// RFC 0025: native ownership of summary transport and durable checkpoints.
module;
module loom.orchestration.agent.utils;

import std;
import loom.services.compact.context_compaction;
import loom.services.compact.model_context;
import loom.query.wire_protocol;
import loom.types.tool_types;
import loom.types.types;

namespace loom::tools::agent::utils {
namespace {
namespace compact = loom::services::compact;
namespace api = loom::services::api;

std::optional<loom::core::ResolvedModelContext> resolved_agent_model_context(
    const std::shared_ptr<const loom::core::ApiExecutionContext>& context, std::string_view model) {
    if (!context || !context->model_context_config) return std::nullopt;
    const auto wire = loom::query::wire::wire_api_from_string(context->wire_api)
        .value_or(loom::query::wire::WireApi::Messages);
    return compact::resolve_model_context(*context->model_context_config, model, context->base_url,
        loom::query::wire::wire_api_name(wire), static_cast<std::uint32_t>(std::min<std::uint64_t>(
            context->context_output_tokens, std::numeric_limits<std::uint32_t>::max())));
}

std::string tool_schema(const CreateMessageRequest& request) {
    std::string schema = "[";
    for (const auto& tool : request.tools) {
        if (schema.size() > 1) schema += ',';
        schema += std::format(R"({{"name":"{}","description":"{}","input_schema":{}}})",
            json_escape_string(tool.name), json_escape_string(tool.description),
            tool.input_schema_json.empty() ? "{}" : tool.input_schema_json);
    }
    return schema + ']';
}

loom::core::Error summary_error(const api::errors::ApiErrorDetails& details) {
    const auto size_error = agent_context_failure(details) != compact::ContextFailure::None;
    auto code = size_error ? loom::core::ErrorCode::ContextWindowExceeded
                          : loom::core::ErrorCode::InvalidRequest;
    if (!size_error) {
        switch (details.category) {
            case api::errors::ApiErrorCategory::NetworkError: code = loom::core::ErrorCode::ConnectionFailed; break;
            case api::errors::ApiErrorCategory::Authentication: code = loom::core::ErrorCode::AuthenticationFailed; break;
            case api::errors::ApiErrorCategory::RateLimited: code = loom::core::ErrorCode::RateLimited; break;
            case api::errors::ApiErrorCategory::Overloaded: code = loom::core::ErrorCode::OverloadedError; break;
            default: break;
        }
    }
    return loom::core::Error::make(code, details.error_message);
}

bool valid_snapshot(loom::utils::json::JsonVal root, std::vector<Message>& messages,
                    std::optional<std::size_t>& anchor) {
    if (root.get("kind").as_str() != "context-checkpoint" || root.get("version").as_int() != 1)
        return false;
    const auto snapshot = root.get("snapshot");
    if (!snapshot.is_arr()) return false;
    bool valid = true;
    std::unordered_set<std::string> pending;
    snapshot.iter([&](auto value) {
        const auto role = value.get("role").as_str();
        if ((role != "user" && role != "assistant") || !value.get("content").is_arr()) {
            valid = false;
            return;
        }
        value.get("content").iter([&](auto block) {
            const auto type = block.get("type").as_str();
            if (type != "text" && type != "tool_use" && type != "tool_result" &&
                type != "thinking" && type != "redacted_thinking" && type != "image" && type != "document")
                valid = false;
            if (type == "image" || type == "document") {
                const auto source = block.get("source");
                if (source.get("type").as_str() != "base64" ||
                    !source.get("media_type").is_str() || !source.get("data").is_str()) valid = false;
            }
        });
        auto parsed = message_from_json_value(value);
        if (!parsed || parsed->content.empty()) { valid = false; return; }
        for (const auto& block : parsed->content) {
            if (block.type == ContentBlockType::ToolUse) {
                if (role != "assistant" || block.tool_use_id.empty() ||
                    !pending.insert(block.tool_use_id).second) valid = false;
            } else if (block.type == ContentBlockType::ToolResult) {
                if (role != "user" || !pending.erase(block.tool_use_id)) valid = false;
            }
        }
        messages.push_back(std::move(*parsed));
    });
    if (!valid || messages.empty() || !pending.empty()) return false;
    if (!compact::valid_checkpoint(message_content_text(messages.front()))) return false;
    auto index = root.get("requestAnchor");
    if (!index.valid()) return false;
    if (index.is_num()) {
        const auto value = index.as_int();
        if (value < 0 || static_cast<std::size_t>(value) >= messages.size() ||
            messages[static_cast<std::size_t>(value)].role != "user") return false;
        anchor = static_cast<std::size_t>(value);
    } else if (index.valid() && !index.is_null()) return false;
    return true;
}
} // namespace

AgentReplayContext replay_agent_context(const std::vector<std::string>& entries) {
    AgentReplayContext replay;
    for (const auto& entry : entries) {
        auto parsed = loom::utils::json::parse(entry);
        if (!parsed || !parsed->root().is_obj()) continue;
        auto root = parsed->root();
        if (root.get("kind").as_str() == "context-checkpoint") {
            std::vector<Message> snapshot;
            std::optional<std::size_t> anchor;
            if (valid_snapshot(root, snapshot, anchor)) {
                replay.messages = std::move(snapshot);
                replay.request_anchor = anchor;
                replay.checkpoint = true;
            }
            continue;
        }
        if (auto message = message_from_json_value(root)) {
            if (root.get("requestAnchor").is_bool() && root.get("requestAnchor").as_bool())
                replay.request_anchor = replay.messages.size();
            replay.messages.push_back(std::move(*message));
        }
    }
    return replay;
}

compact::ContextPolicy agent_context_policy(
    const std::shared_ptr<const loom::core::ApiExecutionContext>& context, std::string_view model) {
    if (!context) return {};
    if (const auto resolved = resolved_agent_model_context(context, model))
        return {resolved->window_tokens, resolved->output_tokens, context->context_compaction_threshold};
    return {context->context_window_tokens, context->context_output_tokens,
        context->context_compaction_threshold};
}

std::uint64_t estimate_agent_context(const CreateMessageRequest& request,
    std::optional<std::size_t> anchor, compact::UsageObservation observed) {
    const auto messages = agent_core_messages(request.messages);
    const auto schema = tool_schema(request);
    return compact::estimate_request_tokens({messages, request.system_prompt.value_or(""),
        schema, anchor, observed});
}

compact::ContextFailure agent_context_failure(const api::errors::ApiErrorDetails& error) {
    // Encode the provider fields so shared classification sees codes as well as text.
    const auto body = std::format(R"({{"error":{{"type":"{}","code":"{}","message":"{}"}}}})",
        json_escape_string(error.error_type), json_escape_string(error.error_code),
        json_escape_string(error.error_message));
    return compact::classify_context_failure(error.http_status, body);
}

loom::core::Result<void> compact_agent_context(api::MessagesClient& client,
    const CreateMessageRequest& request,
    const std::shared_ptr<const loom::core::ApiExecutionContext>& context,
    std::string_view agent_id, std::vector<Message>& messages,
    std::optional<std::size_t>& anchor, compact::UsageObservation& observed,
    std::string_view reason, const std::function<void(int, int)>& record_usage,
    const compact::CancellationCheck& cancelled, bool require_payload_reduction) {
    const auto prior = loom::tools::agent_runtime::native_agent_store().get(agent_id);
    if (!prior) return std::unexpected(loom::core::Error::make(
        loom::core::ErrorCode::SessionNotFound, "Agent disappeared before compaction"));
    std::optional<std::size_t> original_bytes;
    if (require_payload_reduction) {
        auto size = agent_serialized_request_size(client, request, context);
        if (!size) return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InvalidRequest, size.error().message()));
        original_bytes = *size;
    }
    const auto core_messages = agent_core_messages(messages);
    const auto schema = tool_schema(request);
    const auto system = request.system_prompt.value_or("");
    const auto resolved = resolved_agent_model_context(context, request.model);
    const auto summarize = [&](const compact::SummaryRequest& input)
        -> loom::core::Result<compact::SummaryResponse> {
        CreateMessageRequest summary;
        summary.model = request.model;
        summary.system_prompt = input.instructions;
        summary.messages = {Message::from_text("user", input.transcript)};
        summary.max_tokens = static_cast<int>(resolved && resolved->output_cap
            ? std::min(input.max_output_tokens, *resolved->output_cap) : input.max_output_tokens);
        summary.stream = true;
        auto stream = create_agent_model_stream(client, summary, context);
        if (!stream) return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::ConnectionFailed, stream.error().message()));
        compact::SummaryResponse response;
        struct Charge {
            compact::SummaryResponse& response;
            const std::function<void(int, int)>& record;
            ~Charge() { record(static_cast<int>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(response.usage.input_tokens) + response.usage.cache_creation_tokens +
                response.usage.cache_read_tokens, std::numeric_limits<int>::max())),
                static_cast<int>(response.usage.output_tokens)); }
        } charge{response, record_usage};
        bool stopped = false;
        std::unordered_set<int> active_blocks;
        bool invalid_blocks = false;
        std::string stop_reason;
        while (true) {
            if (cancelled && cancelled()) return std::unexpected(loom::core::Error::make(
                loom::core::ErrorCode::InternalError, "Agent cancelled during context compaction"));
            auto event = stream->next_event();
            if (event && !event->has_value()) {
                if (!stream->is_finished()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                // Terminal publication stabilizes the queue; drain a late final frame.
                event = stream->next_event();
            }
            if (!event) {
                api::errors::ApiErrorDetails details;
                details.error_message = event.error().message();
                return std::unexpected(summary_error(details));
            }
            if (!event->has_value()) {
                if (stream->has_error()) return std::unexpected(summary_error(
                    stream->error_details().value_or(api::errors::ApiErrorDetails{})));
                break;
            }
            const auto& value = **event;
            if (value.type == api::StreamEventType::Error)
                return std::unexpected(summary_error(value.error));
            if (value.type == api::StreamEventType::MessageStart) {
                if (value.input_tokens) response.usage.input_tokens = std::max(0, *value.input_tokens);
                if (value.cache_creation_tokens) response.usage.cache_creation_tokens = std::max(0, *value.cache_creation_tokens);
                if (value.cache_read_tokens) response.usage.cache_read_tokens = std::max(0, *value.cache_read_tokens);
            } else if (value.type == api::StreamEventType::ContentBlockStart) {
                if (stopped) invalid_blocks = true;
                if (!active_blocks.insert(value.block_index).second) invalid_blocks = true;
                if (value.block_type == api::StreamContentBlockType::ToolUse) response.has_tool_use = true;
                if (value.block_type == api::StreamContentBlockType::Text) response.text += value.block_text;
            } else if (value.type == api::StreamEventType::ContentBlockDelta &&
                       value.delta.type == api::StreamContentBlockType::Text) {
                if (!active_blocks.contains(value.block_index) || stopped) invalid_blocks = true;
                response.text += value.delta.text;
            } else if (value.type == api::StreamEventType::ContentBlockStop) {
                if (active_blocks.erase(value.block_index) != 1) invalid_blocks = true;
            } else if (value.type == api::StreamEventType::MessageDelta) {
                const auto& usage = value.message_delta.usage;
                if (usage.input_tokens) response.usage.input_tokens = std::max(0, *usage.input_tokens);
                if (usage.output_tokens) response.usage.output_tokens = std::max(0, *usage.output_tokens);
                if (usage.cache_creation_tokens) response.usage.cache_creation_tokens = std::max(0, *usage.cache_creation_tokens);
                if (usage.cache_read_tokens) response.usage.cache_read_tokens = std::max(0, *usage.cache_read_tokens);
                if (value.message_delta.stop_reason) stop_reason = *value.message_delta.stop_reason;
            } else if (value.type == api::StreamEventType::MessageStop) {
                if (!active_blocks.empty()) invalid_blocks = true;
                stopped = true;
            }
        }
        response.complete = stopped && stop_reason == "end_turn" && active_blocks.empty() && !invalid_blocks;
        return response;
    };
    auto result = compact::compact_context({core_messages, system, schema, anchor, observed},
        agent_context_policy(context, request.model), summarize, cancelled);
    if (!result) return std::unexpected(result.error());
    std::vector<Message> replacement{Message::from_text("user", result->summary)};
    std::optional<std::size_t> new_anchor;
    for (const auto index : result->retained_indices) {
        if (index >= messages.size()) return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InternalError, "Invalid native compaction retention index"));
        if (anchor == index) new_anchor = replacement.size();
        replacement.push_back(messages[index]);
    }
    if (original_bytes) {
        auto candidate = request;
        candidate.messages = replacement;
        auto size = agent_serialized_request_size(client, candidate, context);
        if (!size || *size >= *original_bytes) return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::ContextWindowExceeded,
            "Compaction cannot reduce the serialized request payload; reduce attachments, tools or input"));
    }
    std::string snapshot = "[";
    for (const auto& message : replacement) {
        if (snapshot.size() > 1) snapshot += ',';
        snapshot += message_json_object(message);
    }
    snapshot += ']';
    const auto checkpoint = std::format(
        R"({{"kind":"context-checkpoint","version":1,"reason":"{}","beforeTokens":{},"afterTokens":{},"requestAnchor":{},"snapshot":{}}})",
        json_escape_string(reason), result->before_tokens, result->after_tokens,
        new_anchor ? std::to_string(*new_anchor) : "null", snapshot);
    auto committed = loom::tools::agent_runtime::native_agent_store().append_checkpoint_checked(
        agent_id, prior->sidechain_entries.size(), checkpoint);
    if (!committed) return std::unexpected(loom::core::Error::make(
        loom::core::ErrorCode::SessionCorrupted, "Cannot commit agent checkpoint: " + committed.error()));
    messages = std::move(replacement);
    anchor = new_anchor;
    observed = {};
    return {};
}
} // namespace loom::tools::agent::utils
