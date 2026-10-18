// Subagent wire adaptation. The services client owns HTTP lifetime and
// cancellation; the shared wire backend owns OpenAI serialization/decoding.
module;
#include <cstdlib>

module loom.orchestration.agent.utils;

import std;
import loom.query.wire_protocol;
import loom.query.wire_openai;
import loom.services.api.bootstrap;
import loom.services.api.streaming;
import loom.types.types;

namespace loom::tools::agent::utils {
namespace {
namespace api = loom::services::api;
namespace wire = loom::query::wire;

[[nodiscard]] loom::core::ContentBlock wire_content(const api::ContentBlock& block) {
    switch (block.type) {
        case api::ContentBlockType::ToolUse: {
            loom::core::ToolUseBlock value;
            value.id.value = block.tool_use_id;
            value.name = block.tool_name;
            value.input_json = block.tool_input_json;
            return value;
        }
        case api::ContentBlockType::ToolResult: {
            loom::core::ToolResultBlock value;
            value.tool_use_id.value = block.tool_use_id;
            value.content = block.text;
            value.is_error = block.is_error;
            return value;
        }
        case api::ContentBlockType::Image: {
            loom::core::ImageBlock value;
            value.media_type = block.media_type;
            value.data = block.image_data;
            return value;
        }
        case api::ContentBlockType::Document:
            return loom::core::DocumentBlock{block.media_type, block.image_data};
        case api::ContentBlockType::Thinking:
            return loom::core::ThinkingBlock{block.thinking, block.signature};
        case api::ContentBlockType::RedactedThinking:
            // OpenAI has no signed/redacted thinking representation, matching
            // the shared backend's handling of ordinary thinking blocks.
            return loom::core::ThinkingBlock{};
        case api::ContentBlockType::Text:
            return loom::core::TextBlock{block.text};
    }
    return loom::core::TextBlock{};
}

[[nodiscard]] wire::RequestInput wire_input(const api::CreateMessageRequest& request) {
    wire::RequestInput input;
    input.model = request.model;
    input.max_tokens = static_cast<std::uint32_t>(request.max_tokens);
    input.stream = true;
    input.system_prompt = request.system_prompt.value_or("");
    input.temperature = request.temperature;
    input.top_p = request.top_p;
    if (request.top_k) input.top_k = static_cast<std::uint32_t>(*request.top_k);
    input.thinking_enabled = request.thinking_budget.has_value();
    if (request.thinking_budget) {
        input.thinking_budget_tokens = static_cast<std::uint32_t>(*request.thinking_budget);
    }
    for (const auto& message : request.messages) {
        std::vector<loom::core::ContentBlock> content;
        for (const auto& block : message.content) content.push_back(wire_content(block));
        if (message.role == "assistant") {
            loom::core::AssistantMessage value;
            value.content = std::move(content);
            input.messages.push_back(std::move(value));
        } else {
            loom::core::UserMessage value;
            value.content = std::move(content);
            input.messages.push_back(std::move(value));
        }
    }
    for (const auto& tool : request.tools) {
        loom::core::ToolDefinition value;
        value.name = tool.name;
        value.description = tool.description;
        value.permission = loom::core::ToolPermission::Execute;
        input.tools.push_back(std::move(value));
        input.tool_schemas.emplace_back(tool.name, tool.input_schema_json);
    }
    return input;
}

/// Each request gets independent indexed accumulation. OpenAI can interleave
/// arguments for multiple tool calls; flush them in index order only once
/// the model has supplied its finish reason and the stream terminator.
class OpenAiAgentDecoder {
    struct ToolCall {
        std::string id;
        std::string name;
        std::string arguments;
    };
    std::shared_ptr<const wire::WireBackend> backend_;
    std::map<std::uint32_t, ToolCall> calls_;
    std::optional<std::uint32_t> current_call_;
    std::string stop_reason_;
    bool started_ = false;
    bool stopped_ = false;
    int block_index_ = 0;

    [[nodiscard]] static loom::utils::Error invalid_stream(std::string message) {
        return loom::utils::Error(loom::utils::ErrorCode::invalid_argument, std::move(message));
    }

public:
    explicit OpenAiAgentDecoder(std::shared_ptr<const wire::WireBackend> backend)
        : backend_(std::move(backend)) {}

    [[nodiscard]] loom::utils::Result<std::vector<api::StreamEvent>> operator()(
        std::string_view event_name, std::string_view data) {
        std::vector<api::StreamEvent> events;
        if (stopped_) return events;
        if (!started_) {
            api::StreamEvent start;
            start.type = api::StreamEventType::MessageStart;
            events.push_back(std::move(start));
            started_ = true;
        }
        for (const auto& delta : backend_->parse_stream_event(event_name, data)) {
            using Kind = wire::StreamDelta::Kind;
            switch (delta.kind) {
                case Kind::TextDelta: {
                    api::StreamEvent start;
                    start.type = api::StreamEventType::ContentBlockStart;
                    start.block_type = api::StreamContentBlockType::Text;
                    start.block_index = block_index_;
                    events.push_back(std::move(start));
                    api::StreamEvent fragment;
                    fragment.type = api::StreamEventType::ContentBlockDelta;
                    fragment.block_index = block_index_;
                    fragment.delta.type = api::StreamContentBlockType::Text;
                    fragment.delta.text = delta.text;
                    events.push_back(std::move(fragment));
                    api::StreamEvent stop;
                    stop.type = api::StreamEventType::ContentBlockStop;
                    stop.block_index = block_index_++;
                    events.push_back(std::move(stop));
                    break;
                }
                case Kind::ToolUseStart: {
                    const auto index = delta.tool_index.value_or(
                        static_cast<std::uint32_t>(calls_.size()));
                    current_call_ = index;
                    auto& call = calls_[index];
                    if (!delta.tool_id.empty()) {
                        if (!call.id.empty() && call.id != delta.tool_id) {
                            return std::unexpected(invalid_stream("OpenAI tool call changed id within one index"));
                        }
                        call.id = delta.tool_id;
                    }
                    if (!delta.tool_name.empty()) {
                        if (!call.name.empty() && call.name != delta.tool_name) {
                            return std::unexpected(invalid_stream("OpenAI tool call changed name within one index"));
                        }
                        call.name = delta.tool_name;
                    }
                    break;
                }
                case Kind::ToolUseInputDelta: {
                    const auto index = delta.tool_index ? delta.tool_index : current_call_;
                    if (!index || !calls_.contains(*index)) {
                        return std::unexpected(invalid_stream("OpenAI tool arguments arrived without a tool call"));
                    }
                    calls_[*index].arguments += delta.partial_json;
                    break;
                }
                case Kind::Usage: {
                    api::StreamEvent usage;
                    usage.type = api::StreamEventType::MessageDelta;
                    usage.message_delta.usage.input_tokens = static_cast<int>(delta.usage.input_tokens);
                    usage.message_delta.usage.output_tokens = static_cast<int>(delta.usage.output_tokens);
                    events.push_back(std::move(usage));
                    break;
                }
                case Kind::MessageStop: {
                    if (!delta.stop_reason.empty()) {
                        stop_reason_ = delta.stop_reason;
                        break;
                    }
                    if (stop_reason_.empty()) {
                        return std::unexpected(invalid_stream("OpenAI stream ended without a finish reason"));
                    }
                    for (const auto& [index, call] : calls_) {
                        (void)index;
                        if (call.id.empty() || call.name.empty()) {
                            return std::unexpected(invalid_stream("OpenAI stream ended with an incomplete tool call"));
                        }
                        const auto arguments = call.arguments.empty() ? std::string("{}") : call.arguments;
                        auto parsed = loom::utils::json::parse(arguments);
                        if (!parsed || !parsed->root().is_obj()) {
                            return std::unexpected(invalid_stream("OpenAI stream ended with invalid tool arguments"));
                        }
                        api::StreamEvent start;
                        start.type = api::StreamEventType::ContentBlockStart;
                        start.block_type = api::StreamContentBlockType::ToolUse;
                        start.block_index = block_index_;
                        start.tool_use_id = call.id;
                        start.tool_name = call.name;
                        events.push_back(std::move(start));
                        api::StreamEvent fragment;
                        fragment.type = api::StreamEventType::ContentBlockDelta;
                        fragment.block_index = block_index_;
                        fragment.delta.type = api::StreamContentBlockType::ToolUse;
                        fragment.delta.partial_json = arguments;
                        events.push_back(std::move(fragment));
                        api::StreamEvent stop;
                        stop.type = api::StreamEventType::ContentBlockStop;
                        stop.block_index = block_index_++;
                        events.push_back(std::move(stop));
                    }
                    api::StreamEvent reason;
                    reason.type = api::StreamEventType::MessageDelta;
                    reason.message_delta.stop_reason = stop_reason_;
                    events.push_back(std::move(reason));
                    api::StreamEvent stop;
                    stop.type = api::StreamEventType::MessageStop;
                    events.push_back(std::move(stop));
                    stopped_ = true;
                    break;
                }
                case Kind::Error:
                    return std::unexpected(invalid_stream(delta.text));
                case Kind::ThinkingDelta:
                case Kind::BlockStop:
                case Kind::Ignored:
                    break;
            }
        }
        return events;
    }
};
} // namespace

loom::services::api::MessagesClient::Config agent_api_client_config(
    const std::shared_ptr<const loom::core::ApiExecutionContext>& context) {
    if (!context) return loom::services::api::get_default_client().config();
    api::MessagesClient::Config config;
    config.base_url = context->base_url;
    config.api_key = context->api_key;
    config.auth_token = context->auth_token;
    config.api_version = context->api_version;
    config.timeout = context->timeout;
    return config;
}

loom::utils::Result<loom::services::api::StreamParser> create_agent_model_stream(
    api::MessagesClient& client, const CreateMessageRequest& request,
    const std::shared_ptr<const loom::core::ApiExecutionContext>& context) {
    std::string protocol;
    if (context) protocol = context->wire_api;
    else if (const char* value = std::getenv("LOOM_WIRE_API")) protocol = value;
    if (wire::wire_api_from_string(protocol) != wire::WireApi::OpenAi) {
        // Preserve enterprise routing, signing, and credential invalidation.
        return client.create_message_stream(request);
    }
    const auto& config = client.config();
    std::vector<std::pair<std::string, std::string>> headers;
    const auto& credential = config.auth_token.empty() ? config.api_key : config.auth_token;
    if (!credential.empty()) headers.emplace_back("Authorization", "Bearer " + credential);
    auto backend = std::make_shared<wire::OpenAiWireBackend>(config.base_url, std::move(headers));
    auto prepared = backend->prepare(wire_input(request));
    if (!prepared) {
        return std::unexpected(loom::utils::Error(
            loom::utils::ErrorCode::invalid_argument, prepared.error()));
    }
    std::vector<std::string> raw_headers;
    for (const auto& [name, value] : prepared->headers) raw_headers.push_back(name + ": " + value);
    return client.create_prepared_stream(std::move(prepared->url), std::move(prepared->body),
        std::move(raw_headers), OpenAiAgentDecoder(std::move(backend)));
}
} // namespace loom::tools::agent::utils
