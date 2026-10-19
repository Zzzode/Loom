// SSE Stream Parser - Complete Server-Sent Events handling for Messages API
module;
#include <cstdlib>

export module loom.services.api.streaming;

import std;

import loom.serdes.json;
import loom.utils.error;
import loom.services.api.errors;

export namespace loom::services::api {

using loom::services::api::errors::ApiErrorDetails;
using loom::services::api::errors::ErrorFactory;
using loom::utils::Result;

// =========================================================================
// Stream Event Types
// =========================================================================

enum class StreamEventType {
    MessageStart,
    MessageDelta,
    MessageStop,
    ContentBlockStart,
    ContentBlockDelta,
    ContentBlockStop,
    Ping,
    Error,
    Other
};

// =========================================================================
// Content Block Types for Streaming
// =========================================================================

enum class StreamContentBlockType {
    Text,
    ToolUse,
    Thinking,
    Unknown
};

// =========================================================================
// Stream Content Delta
// =========================================================================

struct StreamContentDelta {
    StreamContentBlockType type = StreamContentBlockType::Text;
    std::string text;
    std::string partial_json;  // For tool use inputs
    std::string thinking;
};

// =========================================================================
// Stream Token Usage
// =========================================================================

struct StreamTokenUsage {
    std::optional<int> output_tokens;
    std::optional<int> input_tokens;
    std::optional<int> cache_creation_tokens;
    std::optional<int> cache_read_tokens;
};

// =========================================================================
// Stream Message Delta
// =========================================================================

struct StreamMessageDelta {
    std::optional<std::string> stop_reason;
    std::optional<std::string> stop_sequence;
    StreamTokenUsage usage;
};

// =========================================================================
// Stream Event
// =========================================================================

struct StreamEvent {
    StreamEventType type = StreamEventType::Other;
    std::string event_id;

    // MessageStart fields
    std::string message_id;
    std::string model;
    std::string role;

    // ContentBlockStart fields
    int block_index = -1;
    StreamContentBlockType block_type = StreamContentBlockType::Text;
    std::string block_text;
    std::string tool_name;
    std::string tool_use_id;

    // ContentBlockDelta fields
    StreamContentDelta delta;

    // MessageDelta fields
    StreamMessageDelta message_delta;

    // Error fields
    ApiErrorDetails error;

    // MessageStart usage; appended to preserve aggregate field ordering.
    std::optional<int> input_tokens;
    std::optional<int> cache_creation_tokens;
    std::optional<int> cache_read_tokens;
};

// =========================================================================
// SSE Buffer
// =========================================================================

class SseBuffer {
public:
    void append(std::string_view data) {
        buffer_ += data;
    }

    [[nodiscard]] std::optional<std::pair<std::string, std::string>> next_event() {
        while (true) {
            const auto newline_pos = buffer_.find('\n');
            if (newline_pos == std::string::npos) return std::nullopt;
            auto line = buffer_.substr(0, newline_pos);
            buffer_.erase(0, newline_pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) {
                if (event_type_.empty() && event_data_.empty()) continue;
                auto event = std::make_pair(std::move(event_type_), std::move(event_data_));
                event_type_.clear();
                event_data_.clear();
                return event;
            }
            // Keep partial-frame fields between next_event() calls: CURL may
            // split anywhere, including between an event line and its data.
            if (line.starts_with("event:")) {
                event_type_ = strip_prefix(line, "event:");
            } else if (line.starts_with("data:")) {
                if (!event_data_.empty()) event_data_ += '\n';
                event_data_ += strip_prefix(line, "data:");
            } else if (line.starts_with("id:")) {
                last_event_id_ = strip_prefix(line, "id:");
            } else if (line.starts_with("retry:")) {
                try {
                    retry_ms_ = std::stoi(std::string(strip_prefix(line, "retry:")));
                } catch (...) {}
            }
        }
    }

    [[nodiscard]] const std::string& last_event_id() const { return last_event_id_; }
    [[nodiscard]] int retry_ms() const { return retry_ms_; }
    void clear() {
        buffer_.clear();
        event_type_.clear();
        event_data_.clear();
    }

private:
    [[nodiscard]] static std::string_view strip_prefix(std::string_view line, std::string_view prefix) {
        auto value = line.substr(prefix.size());
        if (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }
        return value;
    }

    std::string buffer_;
    std::string event_type_;
    std::string event_data_;
    std::string last_event_id_;
    int retry_ms_ = 3000;
};

// =========================================================================
// Stream Event Parser
// =========================================================================

class StreamEventParser {
public:
    [[nodiscard]] static Result<StreamEvent> parse(const std::string& event_type,
                                                    const std::string& event_data) {
        StreamEvent event;
        event.event_id = "";  // Would be set from SSE buffer

        if (event_type == "message_start") {
            return parse_message_start(event_data, event);
        } else if (event_type == "content_block_start") {
            return parse_content_block_start(event_data, event);
        } else if (event_type == "content_block_delta") {
            return parse_content_block_delta(event_data, event);
        } else if (event_type == "content_block_stop") {
            return parse_content_block_stop(event_data, event);
        } else if (event_type == "message_delta") {
            return parse_message_delta(event_data, event);
        } else if (event_type == "message_stop") {
            event.type = StreamEventType::MessageStop;
            return event;
        } else if (event_type == "ping") {
            event.type = StreamEventType::Ping;
            return event;
        } else if (event_type == "error") {
            return parse_error(event_data, event);
        }

        event.type = StreamEventType::Other;
        return event;
    }

private:
    [[nodiscard]] static Result<StreamEvent> parse_message_start(const std::string& data,
                                                                 StreamEvent& event) {
        event.type = StreamEventType::MessageStart;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        auto message = root.get("message");

        event.message_id = std::string(message.get("id").as_str());
        event.model = std::string(message.get("model").as_str());
        event.role = std::string(message.get("role").as_str());
        const auto input_tokens = message.get("usage").get("input_tokens");
        if (input_tokens.is_num()) event.input_tokens = static_cast<int>(input_tokens.as_int());
        const auto cache_creation = message.get("usage").get("cache_creation_input_tokens");
        const auto cache_read = message.get("usage").get("cache_read_input_tokens");
        if (cache_creation.is_num()) event.cache_creation_tokens = static_cast<int>(cache_creation.as_int());
        if (cache_read.is_num()) event.cache_read_tokens = static_cast<int>(cache_read.as_int());

        return event;
    }

    [[nodiscard]] static Result<StreamEvent> parse_content_block_start(const std::string& data,
                                                                       StreamEvent& event) {
        event.type = StreamEventType::ContentBlockStart;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        event.block_index = static_cast<int>(root.get("index").as_int());

        auto block = root.get("content_block");
        auto type_str = block.get("type").as_str();

        if (type_str == "text") {
            event.block_type = StreamContentBlockType::Text;
            event.block_text = std::string(block.get("text").as_str());
        } else if (type_str == "tool_use") {
            event.block_type = StreamContentBlockType::ToolUse;
            event.tool_use_id = std::string(block.get("id").as_str());
            event.tool_name = std::string(block.get("name").as_str());
        } else if (type_str == "thinking") {
            event.block_type = StreamContentBlockType::Thinking;
            // thinking fields
        }

        return event;
    }

    [[nodiscard]] static Result<StreamEvent> parse_content_block_delta(const std::string& data,
                                                                       StreamEvent& event) {
        event.type = StreamEventType::ContentBlockDelta;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        event.block_index = static_cast<int>(root.get("index").as_int());

        auto delta = root.get("delta");
        auto delta_type = delta.get("type").as_str();

        if (delta_type == "text_delta") {
            event.delta.type = StreamContentBlockType::Text;
            event.delta.text = std::string(delta.get("text").as_str());
        } else if (delta_type == "input_json_delta") {
            event.delta.type = StreamContentBlockType::ToolUse;
            event.delta.partial_json = std::string(delta.get("partial_json").as_str());
        } else if (delta_type == "thinking_delta") {
            event.delta.type = StreamContentBlockType::Thinking;
            event.delta.thinking = std::string(delta.get("thinking").as_str());
        }

        return event;
    }

    [[nodiscard]] static Result<StreamEvent> parse_content_block_stop(const std::string& data,
                                                                      StreamEvent& event) {
        event.type = StreamEventType::ContentBlockStop;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        event.block_index = static_cast<int>(root.get("index").as_int());

        return event;
    }

    [[nodiscard]] static Result<StreamEvent> parse_message_delta(const std::string& data,
                                                                 StreamEvent& event) {
        event.type = StreamEventType::MessageDelta;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        auto delta = root.get("delta");

        auto stop_reason = delta.get("stop_reason");
        if (stop_reason.valid()) {
            event.message_delta.stop_reason = std::string(stop_reason.as_str());
        }

        auto stop_seq = delta.get("stop_sequence");
        if (stop_seq.valid()) {
            event.message_delta.stop_sequence = std::string(stop_seq.as_str());
        }

        auto usage = root.get("usage");
        if (usage.valid()) {
            const auto cache_creation = usage.get("cache_creation_input_tokens");
            const auto cache_read = usage.get("cache_read_input_tokens");
            if (cache_creation.is_num()) event.message_delta.usage.cache_creation_tokens = static_cast<int>(cache_creation.as_int());
            if (cache_read.is_num()) event.message_delta.usage.cache_read_tokens = static_cast<int>(cache_read.as_int());
            const auto input_tokens = usage.get("input_tokens");
            if (input_tokens.is_num()) {
                event.message_delta.usage.input_tokens = static_cast<int>(input_tokens.as_int());
            }
            event.message_delta.usage.output_tokens =
                static_cast<int>(usage.get("output_tokens").as_int());
        }

        return event;
    }

    [[nodiscard]] static Result<StreamEvent> parse_error(const std::string& data,
                                                         StreamEvent& event) {
        event.type = StreamEventType::Error;
        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) {
            return std::unexpected(doc_result.error());
        }

        auto root = doc_result->root();
        auto error = root.get("error");

        event.error = ApiErrorDetails{};
        event.error.error_type = std::string(error.get("type").as_str());
        event.error.error_message = std::string(error.get("message").as_str());
        event.error.error_code = std::string(error.get("code").as_str());

        return event;
    }
};

// =========================================================================
// Connection State
// =========================================================================

enum class ConnectionState {
    Idle,
    Connecting,
    Connected,
    Reconnecting,
    Disconnected,
    Finished,
    Error
};

// =========================================================================
// Stream Configuration
// =========================================================================

struct StreamConfig {
    std::chrono::milliseconds connect_timeout{30000};
    std::chrono::milliseconds read_timeout{120000};
    std::chrono::milliseconds heartbeat_interval{30000};
    int max_reconnect_attempts = 5;
    std::chrono::milliseconds base_reconnect_delay{1000};
    std::chrono::milliseconds max_reconnect_delay{30000};
    bool enable_reconnect = true;
};

// =========================================================================
// Stream Statistics
// =========================================================================

struct StreamStatistics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    int total_events = 0;
    int text_blocks = 0;
    int tool_use_blocks = 0;
    int reconnects = 0;
    int input_tokens = 0;
    int output_tokens = 0;
    std::chrono::milliseconds time_to_first_token{0};

    [[nodiscard]] std::chrono::milliseconds total_duration() const {
        if (end_time == std::chrono::steady_clock::time_point{}) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
};

// =========================================================================
// Accumulated Stream State
// =========================================================================

struct StreamState {
    std::string message_id;
    std::string model;
    std::string full_text;
    std::vector<std::pair<std::string, std::string>> tool_uses;  // id -> json
    std::string stop_reason;
    int input_tokens = 0;
    int output_tokens = 0;
    bool is_complete = false;
    bool has_error = false;
};

// =========================================================================
// Main Stream Parser
// =========================================================================

// Coordinates consumer/client cancellation without detaching network work.
class StreamTransfer {
public:
    template<typename Producer>
    explicit StreamTransfer(Producer&& producer)
        : worker_(std::forward<Producer>(producer)) {}

    ~StreamTransfer() { stop_and_join(); }

    void request_stop() noexcept { worker_.request_stop(); }

    void stop_and_join() {
        request_stop();
        std::scoped_lock lock(join_mutex_);
        if (worker_.joinable()) worker_.join();
    }

private:
    std::mutex join_mutex_;
    std::jthread worker_;
};

class StreamParser {
public:
    using EventDecoder = std::function<Result<std::vector<StreamEvent>>(
        std::string_view event_name, std::string_view data)>;

private:
    struct SharedState {
        explicit SharedState(StreamConfig cfg, EventDecoder decode)
            : config(std::move(cfg)), decoder(std::move(decode)) {}

        StreamConfig config;
        ConnectionState connection = ConnectionState::Idle;
        SseBuffer buffer;
        EventDecoder decoder;
        std::deque<StreamEvent> pending_events;
        StreamStatistics statistics;
        StreamState stream_state;
        std::optional<ApiErrorDetails> error_details;
        int reconnect_attempts = 0;
        mutable std::mutex mutex;
    };

    struct TransferOwner {
        explicit TransferOwner(std::shared_ptr<StreamTransfer> request)
            : transfer(std::move(request)) {}
        ~TransferOwner() { transfer->stop_and_join(); }
        std::shared_ptr<StreamTransfer> transfer;
    };

public:
    explicit StreamParser(StreamConfig config = {}, EventDecoder decoder = {})
        : state_(std::make_shared<SharedState>(std::move(config), std::move(decoder))) {}

    // Consumer copies share ownership of the transfer; the producer only holds
    // parser state, so dropping the last consumer stops and joins the request.
    void own_transfer(std::shared_ptr<StreamTransfer> transfer) {
        transfer_ = std::make_shared<TransferOwner>(std::move(transfer));
    }

    // Factory method for parser setup; MessagesClient starts the HTTP transfer.
    static std::optional<StreamParser> create(
        const std::string&,
        const std::string&,
        const std::vector<std::string>&,
        std::chrono::milliseconds timeout) {
        StreamParser parser;
        {
            std::scoped_lock lock(parser.state_->mutex);
            parser.state_->config.connect_timeout = timeout;
        }
        parser.feed({});
        return parser;
    }

    // Feed raw data
    void feed(std::string_view data) {
        std::scoped_lock lock(state_->mutex);
        state_->buffer.append(data);
    }

    // Get next parsed event
    [[nodiscard]] Result<std::optional<StreamEvent>> next_event() {
        std::scoped_lock lock(state_->mutex);
        while (state_->pending_events.empty()) {
            auto raw_event = state_->buffer.next_event();
            if (!raw_event) return std::nullopt;
            auto& [event_type, event_data] = *raw_event;
            if (state_->decoder) {
                auto events = state_->decoder(event_type, event_data);
                if (!events) return std::unexpected(events.error());
                for (auto& event : *events) state_->pending_events.push_back(std::move(event));
            } else {
                auto event = StreamEventParser::parse(event_type, event_data);
                if (!event) return std::unexpected(event.error());
                state_->pending_events.push_back(std::move(*event));
            }
        }
        auto event = std::move(state_->pending_events.front());
        state_->pending_events.pop_front();
        update_statistics_locked(*state_, event);
        update_stream_state_locked(*state_, event);
        return event;
    }

    // Process all available events with callback
    template<typename Callback>
    requires std::invocable<Callback, const StreamEvent&>
    void process_events(Callback&& callback) {
        while (auto event = next_event()) {
            if (!event->has_value()) break;
            callback(**event);
            if ((*event)->type == StreamEventType::MessageStop ||
                (*event)->type == StreamEventType::Error) {
                break;
            }
        }
    }

    // State access
    [[nodiscard]] ConnectionState connection_state() const {
        std::scoped_lock lock(state_->mutex);
        return state_->connection;
    }
    [[nodiscard]] bool is_connected() const { return connection_state() == ConnectionState::Connected; }
    [[nodiscard]] bool is_finished() const {
        const auto state = connection_state();
        return state == ConnectionState::Finished || state == ConnectionState::Error;
    }
    [[nodiscard]] bool has_error() const {
        std::scoped_lock lock(state_->mutex);
        return state_->stream_state.has_error;
    }
    [[nodiscard]] std::optional<ApiErrorDetails> error_details() const {
        std::scoped_lock lock(state_->mutex);
        return state_->error_details;
    }

    // Statistics
    [[nodiscard]] StreamStatistics statistics() const {
        std::scoped_lock lock(state_->mutex);
        return state_->statistics;
    }

    // Accumulated state
    [[nodiscard]] StreamState stream_state() const {
        std::scoped_lock lock(state_->mutex);
        return state_->stream_state;
    }
    [[nodiscard]] std::string full_text() const {
        std::scoped_lock lock(state_->mutex);
        return state_->stream_state.full_text;
    }

    // Reconnection
    [[nodiscard]] bool can_reconnect() const {
        std::scoped_lock lock(state_->mutex);
        return state_->config.enable_reconnect &&
               state_->reconnect_attempts < state_->config.max_reconnect_attempts;
    }

    [[nodiscard]] std::chrono::milliseconds next_reconnect_delay() const {
        std::scoped_lock lock(state_->mutex);
        // Exponential backoff with jitter
        auto delay = state_->config.base_reconnect_delay * (1 << state_->reconnect_attempts);
        if (delay > state_->config.max_reconnect_delay) {
            delay = state_->config.max_reconnect_delay;
        }
        // Add jitter (±20%)
        auto jitter = static_cast<double>(std::rand()) / RAND_MAX * 0.4 - 0.2;
        return std::chrono::milliseconds(static_cast<long long>(delay.count() * (1 + jitter)));
    }

    void reset_reconnect_attempts() {
        std::scoped_lock lock(state_->mutex);
        state_->reconnect_attempts = 0;
    }

    void mark_reconnect() {
        std::scoped_lock lock(state_->mutex);
        ++state_->reconnect_attempts;
        ++state_->statistics.reconnects;
    }

    // Lifecycle
    void start() {
        std::scoped_lock lock(state_->mutex);
        state_->connection = ConnectionState::Connected;
        state_->statistics.start_time = std::chrono::steady_clock::now();
    }

    void finish() {
        std::scoped_lock lock(state_->mutex);
        state_->connection = ConnectionState::Finished;
        state_->statistics.end_time = std::chrono::steady_clock::now();
    }

    void set_error(const ApiErrorDetails& error) {
        std::scoped_lock lock(state_->mutex);
        state_->connection = ConnectionState::Error;
        state_->stream_state.has_error = true;
        state_->error_details = error;
        state_->statistics.end_time = std::chrono::steady_clock::now();
    }

    void clear() {
        std::scoped_lock lock(state_->mutex);
        state_->buffer.clear();
        state_->pending_events.clear();
        state_->stream_state = StreamState{};
        state_->error_details = std::nullopt;
        state_->statistics = StreamStatistics{};
        state_->reconnect_attempts = 0;
        state_->connection = ConnectionState::Idle;
    }

    [[nodiscard]] std::string last_event_id() const {
        std::scoped_lock lock(state_->mutex);
        return state_->buffer.last_event_id();
    }

private:
    static void update_statistics_locked(SharedState& state, const StreamEvent& event) {
        ++state.statistics.total_events;

        if (state.statistics.total_events == 1) {
            state.statistics.time_to_first_token =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - state.statistics.start_time);
        }

        switch (event.type) {
            case StreamEventType::MessageStart:
                if (event.input_tokens) state.statistics.input_tokens = *event.input_tokens;
                break;

            case StreamEventType::ContentBlockStart:
                if (event.block_type == StreamContentBlockType::Text) {
                    ++state.statistics.text_blocks;
                } else if (event.block_type == StreamContentBlockType::ToolUse) {
                    ++state.statistics.tool_use_blocks;
                }
                break;

            case StreamEventType::MessageDelta:
                if (event.message_delta.usage.input_tokens) {
                    state.statistics.input_tokens = *event.message_delta.usage.input_tokens;
                }
                if (event.message_delta.usage.output_tokens) {
                    state.statistics.output_tokens = *event.message_delta.usage.output_tokens;
                }
                break;

            default:
                break;
        }
    }

    static void update_stream_state_locked(SharedState& state, const StreamEvent& event) {
        switch (event.type) {
            case StreamEventType::MessageStart:
                state.stream_state.message_id = event.message_id;
                state.stream_state.model = event.model;
                if (event.input_tokens) state.stream_state.input_tokens = *event.input_tokens;
                break;

            case StreamEventType::ContentBlockDelta:
                if (event.delta.type == StreamContentBlockType::Text) {
                    state.stream_state.full_text += event.delta.text;
                }
                break;

            case StreamEventType::MessageDelta:
                if (event.message_delta.usage.input_tokens) {
                    state.stream_state.input_tokens = *event.message_delta.usage.input_tokens;
                }
                if (event.message_delta.stop_reason) {
                    state.stream_state.stop_reason = *event.message_delta.stop_reason;
                }
                if (event.message_delta.usage.output_tokens) {
                    state.stream_state.output_tokens = *event.message_delta.usage.output_tokens;
                }
                break;

            case StreamEventType::MessageStop:
                state.stream_state.is_complete = true;
                break;

            case StreamEventType::Error:
                state.stream_state.has_error = true;
                break;

            default:
                break;
        }
    }

    std::shared_ptr<SharedState> state_;
    std::shared_ptr<TransferOwner> transfer_;
};

// =========================================================================
// Stream Event Type to String
// =========================================================================

[[nodiscard]] inline std::string_view stream_event_type_to_string(StreamEventType type) {
    switch (type) {
        case StreamEventType::MessageStart: return "message_start";
        case StreamEventType::MessageDelta: return "message_delta";
        case StreamEventType::MessageStop: return "message_stop";
        case StreamEventType::ContentBlockStart: return "content_block_start";
        case StreamEventType::ContentBlockDelta: return "content_block_delta";
        case StreamEventType::ContentBlockStop: return "content_block_stop";
        case StreamEventType::Ping: return "ping";
        case StreamEventType::Error: return "error";
        case StreamEventType::Other: return "other";
    }
    return "unknown";
}

} // namespace loom::services::api
