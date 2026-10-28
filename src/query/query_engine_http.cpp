// Implementation unit for loom.query.query_engine — the direct httplib HTTP
// path: non-streaming send_request, SSE stream_single_api_call, response
// parsing, SSE framing (SseEventDecoder::feed), and the free
// api_messages_endpoint helper. This is the ONLY implementation unit that
// textually includes <httplib.h>; keeping it here lets the third-party
// header leave the module interface BMI. Raw httplib types are NOT exported
// by loom.net.http.http (it includes the same header in its own global module
// fragment and only exports loom::utils::HttpClient wrappers), so this TU
// textually includes <httplib.h> exactly like src/services/auth/*.cppm.
module;

#include <httplib.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

module loom.query.query_engine;

import std;

import loom.types.types;
import loom.utils.error;
import loom.serdes.json;
import loom.query.wire_protocol;
import loom.services.compact.model_context;

namespace loom::core {

[[nodiscard]] Result<loom::services::compact::SummaryResponse>
QueryEngine::summarize_context(const loom::services::compact::SummaryRequest& request) {
    if (should_abort() || budget_tracker_.budget_exceeded)
        return std::unexpected(Error::make(ErrorCode::InternalError,
                                           "Compaction cancelled or spend budget exhausted"));
    auto backend = make_wire_backend(true);
    loom::query::wire::RequestInput input;
    input.model = config_.model_params.model;
    input.max_tokens = model_context_.output_cap
                           ? std::min(request.max_output_tokens, *model_context_.output_cap)
                           : request.max_output_tokens;
    input.system_prompt = request.instructions;
    input.stream = false;
    input.thinking_enabled = false;
    input.native_computer_tool = false;
    UserMessage prompt{};
    prompt.content.push_back(TextBlock{request.transcript});
    input.messages.emplace_back(std::move(prompt));
    auto prepared = backend->prepare(input);
    if (!prepared) return std::unexpected(Error::make(ErrorCode::InvalidRequest, prepared.error()));
    // The endpoint helper is declared below; the public protocol helper keeps
    // the same provider path prefix as ordinary requests.
    const auto scheme = prepared->url.find("://");
    const auto slash =
        scheme == std::string::npos ? std::string::npos : prepared->url.find('/', scheme + 3);
    if (scheme == std::string::npos)
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, "Invalid summary endpoint"));
    const auto base = prepared->url.substr(0, slash);
    const auto path = slash == std::string::npos ? std::string{"/"} : prepared->url.substr(slash);
    auto client = std::make_shared<httplib::Client>(base);
#ifdef SO_NOSIGPIPE
    client->set_socket_options([](::socket_t socket) {
        const int enabled = 1;
        ::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
    });
#endif
    client->set_connection_timeout(api_config_.timeout);
    client->set_read_timeout(api_config_.timeout);
    client->set_write_timeout(api_config_.timeout);
    httplib::Headers headers(prepared->headers.begin(), prepared->headers.end());
    set_abort_callback([client] { client->stop(); });
    if (should_abort()) {
        set_abort_callback(nullptr);
        return std::unexpected(Error::make(ErrorCode::InternalError, "Compaction cancelled"));
    }
    if (const auto dump = dump_prompts_path(); dump) {
        std::ofstream out(*dump, std::ios::app);
        out << "{\"type\":\"compaction_request\",\"body\":" << prepared->body << "}\n";
    }
    const auto response = client->Post(path, headers, prepared->body, "application/json");
    set_abort_callback(nullptr);
    if (should_abort())
        return std::unexpected(Error::make(ErrorCode::InternalError, "Compaction cancelled"));
    if (!response)
        return std::unexpected(
            Error::make(ErrorCode::ConnectionFailed,
                        "Compaction HTTP request failed: " + httplib::to_string(response.error())));
    if (response->status >= 400) {
        const auto category =
            loom::services::compact::classify_context_failure(response->status, response->body);
        return std::unexpected(Error::make(
            category == loom::services::compact::ContextFailure::None
                ? classify_http_error(response->status)
                : ErrorCode::ContextWindowExceeded,
            std::format("Compaction API error ({}): {}", response->status, response->body)));
    }
    auto parsed = backend->parse_response(response->body);
    if (!parsed) return std::unexpected(Error::make(ErrorCode::InvalidRequest, parsed.error()));
    account_compaction_usage(parsed->usage);
    loom::services::compact::SummaryResponse result;
    result.usage = parsed->usage;
    result.complete =
        parsed->message.stop_reason == "end_turn" || parsed->message.stop_reason == "stop";
    for (const auto& block : parsed->message.content) {
        if (const auto* text = std::get_if<TextBlock>(&block)) result.text += text->text;
        if (std::holds_alternative<ToolUseBlock>(block)) result.has_tool_use = true;
    }
    if (const auto dump = dump_prompts_path(); dump) {
        std::ofstream out(*dump, std::ios::app);
        out << "{\"type\":\"compaction_response\",\"body\":" << response->body << "}\n";
    }
    return result;
}

namespace {

// OpenSSL can write a TLS close-notify after stop() has shut down the
// connection. Keep the resulting EPIPE local to the request instead of
// terminating the process before the UI restores the terminal.
void suppress_socket_sigpipe(httplib::Client& client) {
#ifdef SO_NOSIGPIPE
    client.set_socket_options([](::socket_t socket) {
        const int enabled = 1;
        ::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
    });
#else
    (void)client;
#endif
}

// Split an already prepared absolute URL without appending a protocol path.
[[nodiscard]] std::expected<ApiMessagesEndpoint, std::string>
prepared_endpoint(std::string_view url) {
    if (!url.starts_with("http://") && !url.starts_with("https://")) {
        return std::unexpected("API URL must include http:// or https://");
    }
    const auto authority = url.find("://") + 3;
    const auto path = url.find('/', authority);
    if (authority == url.size() || path == authority) {
        return std::unexpected("API URL host cannot be empty");
    }
    return ApiMessagesEndpoint{std::string(url.substr(0, path)),
                               path == std::string_view::npos ? "/"
                                                              : std::string(url.substr(path))};
}

}  // namespace

[[nodiscard]] std::expected<ApiMessagesEndpoint, std::string>
api_messages_endpoint(std::string_view base_url) {
    const auto scheme_end = base_url.find("://");
    if (scheme_end == std::string_view::npos) {
        return std::unexpected("API base URL must include http:// or https://");
    }

    const auto authority_start = scheme_end + 3;
    const auto path_start = base_url.find('/', authority_start);
    std::string client_base_url = path_start == std::string_view::npos
                                      ? std::string(base_url)
                                      : std::string(base_url.substr(0, path_start));
    std::string path_prefix = path_start == std::string_view::npos
                                  ? std::string{}
                                  : std::string(base_url.substr(path_start));

    while (client_base_url.size() > scheme_end + 3 && client_base_url.ends_with('/')) {
        client_base_url.pop_back();
    }
    while (!path_prefix.empty() && path_prefix.ends_with('/')) {
        path_prefix.pop_back();
    }
    if (path_prefix == "/") path_prefix.clear();

    if (client_base_url.size() <= scheme_end + 3) {
        return std::unexpected("API base URL host cannot be empty");
    }

    std::string path = path_prefix;
    path += path.ends_with("/v1") ? "/messages" : "/v1/messages";
    return ApiMessagesEndpoint{
        .client_base_url = std::move(client_base_url),
        .path = std::move(path),
    };
}

void prepare_model_metadata(QueryEngineConfig& config) {
    const auto base_url = config.base_url.value_or("");
    const auto selected =
        config.wire_api && !config.wire_api->empty()
            ? *config.wire_api
            : (std::getenv("LOOM_WIRE_API") ? std::getenv("LOOM_WIRE_API") : "messages");
    const auto wire = loom::query::wire::wire_api_from_string(selected).value_or(
        loom::query::wire::WireApi::Messages);
    const auto canonical_wire = std::string(loom::query::wire::wire_api_name(wire));
    const auto& prior = config.model_metadata;
    if (prior && prior->base_url == base_url && prior->wire_api == canonical_wire &&
        prior->api_key == config.api_key && prior->auth_token == config.auth_token)
        return;
    auto snapshot = std::make_shared<QueryEngineConfig::ModelMetadataSnapshot>();
    snapshot->base_url = base_url;
    snapshot->wire_api = canonical_wire;
    snapshot->api_key = config.api_key;
    snapshot->auth_token = config.auth_token;
    // An empty snapshot still records an attempt for this precise identity.
    config.model_metadata = snapshot;
    if ((!base_url.starts_with("http://") && !base_url.starts_with("https://")) ||
        base_url.find_first_of("?#") != std::string::npos ||
        std::ranges::any_of(base_url, [](unsigned char ch) { return ch <= 0x20 || ch == 0x7f; }))
        return;
    const auto authority_start = base_url.find("://") + 3;
    const auto authority_end = base_url.find('/', authority_start);
    if (std::string_view(base_url)
            .substr(authority_start, authority_end == std::string::npos
                                         ? std::string::npos
                                         : authority_end - authority_start)
            .contains('@'))
        return;
    const auto endpoint = api_messages_endpoint(base_url);
    if (!endpoint) return;
    auto path = endpoint->path;
    path.replace(path.size() - std::string_view("messages").size(),
                 std::string_view("messages").size(), "models");
    try {
        httplib::Client client(endpoint->client_base_url);
        suppress_socket_sigpipe(client);
        client.set_connection_timeout(std::chrono::seconds{1});
        client.set_read_timeout(std::chrono::seconds{1});
        client.set_write_timeout(std::chrono::seconds{1});
        client.set_follow_location(false);
        client.set_decompress(true);
        httplib::Headers headers;
        const auto& credential = config.auth_token.empty() ? config.api_key : config.auth_token;
        if (wire == loom::query::wire::WireApi::OpenAi || !config.auth_token.empty()) {
            if (!credential.empty()) headers.emplace("Authorization", "Bearer " + credential);
        } else if (!config.api_key.empty())
            headers.emplace("x-api-key", config.api_key);
        if (wire == loom::query::wire::WireApi::Messages)
            headers.emplace("anthropic-version", "2023-06-01");
        std::string body;
        bool oversized = false;
        std::atomic<bool> timed_out{false};
        std::mutex deadline_mutex;
        std::condition_variable_any deadline_cv;
        std::jthread deadline([&](std::stop_token stop) {
            std::unique_lock lock(deadline_mutex);
            deadline_cv.wait_for(lock, stop, std::chrono::seconds{2}, [] { return false; });
            if (!stop.stop_requested()) {
                timed_out.store(true);
                client.stop();
            }
        });
        const auto response = client.Get(path, headers, [&](const char* data, std::size_t size) {
            constexpr std::size_t limit = 4 * 1024 * 1024;
            if (size > limit - body.size()) {
                oversized = true;
                return false;
            }
            body.append(data, size);
            return true;
        });
        deadline.request_stop();
        deadline_cv.notify_all();
        deadline.join();
        if (!response || oversized || timed_out.load() || response->status < 200 ||
            response->status >= 300)
            return;
        snapshot->limits =
            loom::services::compact::parse_model_metadata(body, base_url, canonical_wire);
    } catch (const std::exception&) {
        // Metadata is best-effort; malformed endpoints and transport failures
        // must not prevent the ordinary model request from being attempted.
    }
}

[[nodiscard]] std::vector<SseEvent> SseEventDecoder::feed(std::string_view chunk) {
    std::vector<SseEvent> out;
    buffer_.append(chunk.data(), chunk.size());
    while (true) {
        const auto lf = buffer_.find("\n\n");
        const auto crlf = buffer_.find("\r\n\r\n");
        const auto boundary = std::min(lf, crlf);
        if (boundary == std::string::npos) break;

        std::string event_block = buffer_.substr(0, boundary);
        buffer_.erase(0, boundary + (boundary == crlf ? 4 : 2));

        std::string event_data;
        std::size_t pos = 0;
        while (pos < event_block.size()) {
            const auto nl = event_block.find('\n', pos);
            std::string line;
            if (nl == std::string::npos) {
                line = event_block.substr(pos);
                pos = event_block.size();
            } else {
                line = event_block.substr(pos, nl - pos);
                pos = nl + 1;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("event:")) {
                current_event_type_ = line.substr(line.starts_with("event: ") ? 7 : 6);
            } else if (line.starts_with("data:")) {
                if (!event_data.empty()) event_data += '\n';
                event_data += line.substr(line.starts_with("data: ") ? 6 : 5);
            }
        }
        if (!event_data.empty()) {
            out.push_back({current_event_type_, std::move(event_data)});
        }
    }
    return out;
}

[[nodiscard]] Result<QueryEngine::ApiCallResult>
QueryEngine::send_request(const QueryOptions& options, bool is_retry_after_compact) {
    if (should_abort()) {
        return std::unexpected(Error::make(ErrorCode::InternalError, "Query aborted"));
    }
    replay_snip_boundaries();
    apply_time_based_microcompact();
    apply_tool_result_budget();

    if (!is_retry_after_compact) {
        auto budget = ensure_context_budget(options);
        if (!budget) return std::unexpected(budget.error());
    }

    auto backend = make_wire_backend();
    const auto sent_input = build_wire_input(options, false);
    auto prepared = backend->prepare(sent_input);
    if (!prepared) {
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, prepared.error()));
    }
    auto endpoint = prepared_endpoint(prepared->url);
    if (!endpoint) {
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, endpoint.error()));
    }
    httplib::Headers headers(prepared->headers.begin(), prepared->headers.end());
    headers.emplace("User-Agent", "LOOM/1.0");
    const auto& body = prepared->body;
    const auto sent_schema = context_schema_text(body);
    capture_context_schema(sent_schema);

    // Create HTTP client (shared_ptr so abort_callback_ can safely call
    // cli->stop() from the main thread even after this function returns).
    auto cli = std::make_shared<httplib::Client>(endpoint->client_base_url);
    suppress_socket_sigpipe(*cli);
    cli->set_connection_timeout(api_config_.timeout);
    cli->set_read_timeout(api_config_.timeout);
    cli->set_write_timeout(api_config_.timeout);

    // Register socket-shutdown callback so abort() (ESC) can interrupt
    // the blocking Post() immediately instead of waiting for the read
    // timeout (up to 120 s).
    set_abort_callback([cli]() { cli->stop(); });
    if (should_abort()) {
        set_abort_callback(nullptr);
        return std::unexpected(Error::make(ErrorCode::InternalError, "Query aborted"));
    }
    auto res = cli->Post(endpoint->path, headers, body, "application/json");
    set_abort_callback(nullptr);

    if (!res) {
        auto err = res.error();
        return std::unexpected(
            Error::make(ErrorCode::ConnectionFailed,
                        std::format("HTTP request failed: {}", httplib::to_string(err))));
    }

    // Check for HTTP errors
    if (res->status >= 400) {
        const auto category =
            loom::services::compact::classify_context_failure(res->status, res->body);
        if (!is_retry_after_compact && category != loom::services::compact::ContextFailure::None) {
            auto compact_result = compact_context_impl(
                "reactive", options,
                category == loom::services::compact::ContextFailure::RequestBytesExceeded
                    ? body.size()
                    : 0,
                false);
            if (!compact_result) return std::unexpected(compact_result.error());
            return send_request(options, /*is_retry_after_compact=*/true);
        }

        return std::unexpected(
            Error::make(classify_http_error(res->status),
                        std::format("API error ({}): {}", res->status, res->body)));
    }

    // Parse response
    auto parsed = backend->parse_response(res->body);
    if (!parsed) {
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, parsed.error()));
    }
    ApiCallResult result;
    result.message = std::move(parsed->message);
    result.usage = parsed->usage;
    observe_context_usage(result.usage, sent_input, sent_schema);
    return result;
}

[[nodiscard]] ErrorCode QueryEngine::classify_http_error(int status) const {
    if (status == 401 || status == 403) {
        return ErrorCode::AuthenticationFailed;
    } else if (status == 429) {
        return ErrorCode::RateLimited;
    } else if (status == 529) {
        return ErrorCode::OverloadedError;
    } else if (status >= 400 && status < 500) {
        return ErrorCode::InvalidRequest;
    } else if (status >= 500) {
        return ErrorCode::InternalError;
    }
    return ErrorCode::InternalError;
}

[[nodiscard]] QueryEngine::StreamCallResult
QueryEngine::stream_single_api_call(const QueryOptions& options, bool is_retry_after_compact) {
    StreamCallResult result;
    replay_snip_boundaries();
    apply_time_based_microcompact();
    apply_tool_result_budget();
    if (!is_retry_after_compact) {
        const auto budget = ensure_context_budget(options);
        if (!budget) {
            result.failed = true;
            result.error_message = budget.error().message;
            if (options.on_event)
                (*options.on_event)(StreamError{"context_compaction_error", result.error_message});
            return result;
        }
    }

    auto backend = make_wire_backend();
    const auto sent_input = build_wire_input(options, true);
    auto prepared = backend->prepare(sent_input);
    if (!prepared) {
        result.failed = true;
        result.error_message = prepared.error();
        if (options.on_event) {
            StreamError event;
            event.error_type = "invalid_request_error";
            event.message = result.error_message;
            (*options.on_event)(event);
        }
        return result;
    }
    httplib::Headers headers(prepared->headers.begin(), prepared->headers.end());
    headers.emplace("User-Agent", "LOOM/1.0");
    const auto& body = prepared->body;
    const auto sent_schema = context_schema_text(body);
    capture_context_schema(sent_schema);

    // ── Dump Prompts: log API request ──
    if (dump_prompts_dir_) {
        auto dp = dump_prompts_path();
        if (dp) {
            std::ofstream ofs(*dp, std::ios::app);
            if (ofs.is_open()) {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
                ofs << "{\"type\":\"request\",\"timestamp\":" << now_ms << ",\"body\":" << body
                    << "}\n";
            }
        }
    }

    auto endpoint = prepared_endpoint(prepared->url);
    if (!endpoint) {
        result.failed = true;
        result.error_message = endpoint.error();
        if (options.on_event) {
            StreamError ev;
            ev.error_type = "invalid_request_error";
            ev.message = result.error_message;
            (*options.on_event)(ev);
        }
        return result;
    }

    // Create HTTP client
    // Create HTTP client (shared_ptr so abort_callback_ can safely call
    // cli->stop() from the main thread even after this function returns).
    auto cli = std::make_shared<httplib::Client>(endpoint->client_base_url);
    suppress_socket_sigpipe(*cli);
    cli->set_connection_timeout(api_config_.timeout);
    cli->set_read_timeout(api_config_.timeout);
    cli->set_write_timeout(api_config_.timeout);

    // SSE parsing state
    SseEventDecoder sse_decoder_;
    std::uint32_t block_index = 0;

    // Content accumulation
    struct BlockAccum {
        ContentBlock block;
        std::string accumulated_text;
        std::string accumulated_json;
    };

    std::vector<BlockAccum> blocks;

    auto emit_stream_error = [&](std::string error_type, std::string message) {
        result.failed = true;
        result.error_message = message.empty() ? error_type : message;
        result.error_type = std::move(error_type);
    };

    auto parse_sse_event = [&](const std::string& event_type, const std::string& data) {
        if (data.empty() || data == "[DONE]") return;

        auto doc_result = loom::utils::json::parse(data);
        if (!doc_result) return;
        auto root = doc_result->root();

        if (event_type == "message_start") {
            auto msg = root.get("message");
            if (msg.valid()) {
                auto id = msg.get("id");
                if (id.valid()) result.message.id.value = std::string(id.as_str());
                auto model = msg.get("model");
                if (model.valid()) result.message.model = std::string(model.as_str());
                auto usage = msg.get("usage");
                if (usage.valid() && usage.is_obj()) {
                    auto input = usage.get("input_tokens");
                    if (input.valid()) {
                        result.usage.input_tokens = static_cast<std::uint32_t>(input.as_int());
                    }
                    auto cache_creation = usage.get("cache_creation_input_tokens");
                    if (cache_creation.valid()) {
                        result.usage.cache_creation_tokens =
                            static_cast<std::uint32_t>(cache_creation.as_int());
                    }
                    auto cache_read = usage.get("cache_read_input_tokens");
                    if (cache_read.valid()) {
                        result.usage.cache_read_tokens =
                            static_cast<std::uint32_t>(cache_read.as_int());
                    }
                }
            }
            if (options.on_event) {
                StreamStart ev;
                ev.message_id = result.message.id;
                ev.model = result.message.model.value_or(config_.model_params.model);
                (*options.on_event)(ev);
            }
        } else if (event_type == "content_block_start") {
            auto idx = root.get("index");
            block_index = idx.valid() ? static_cast<std::uint32_t>(idx.as_int())
                                      : static_cast<std::uint32_t>(blocks.size());
            auto cb = root.get("content_block");
            BlockAccum accum;
            if (cb.valid()) {
                auto type = cb.get("type");
                if (type.valid()) {
                    auto type_str = type.as_str();
                    if (type_str == "text") {
                        accum.block = TextBlock{};
                    } else if (type_str == "tool_use") {
                        ToolUseBlock tub;
                        auto id = cb.get("id");
                        if (id.valid()) tub.id.value = std::string(id.as_str());
                        auto name = cb.get("name");
                        if (name.valid()) tub.name = std::string(name.as_str());
                        accum.block = std::move(tub);
                    } else if (type_str == "thinking") {
                        accum.block = ThinkingBlock{};
                    }
                }
            }
            blocks.push_back(std::move(accum));
            if (options.on_event) {
                ContentBlockStart ev;
                ev.index = block_index;
                ev.block = blocks.back().block;
                (*options.on_event)(ev);
            }
        } else if (event_type == "content_block_delta") {
            auto idx = root.get("index");
            auto cur_idx = idx.valid() ? static_cast<std::uint32_t>(idx.as_int()) : block_index;
            auto delta = root.get("delta");
            if (delta.valid() && cur_idx < blocks.size()) {
                auto type = delta.get("type");
                if (type.valid()) {
                    auto type_str = type.as_str();
                    std::string delta_text;
                    if (type_str == "text_delta") {
                        auto text = delta.get("text");
                        if (text.valid()) {
                            delta_text = std::string(text.as_str());
                            blocks[cur_idx].accumulated_text += delta_text;
                        }
                    } else if (type_str == "input_json_delta") {
                        auto pj = delta.get("partial_json");
                        if (pj.valid()) {
                            delta_text = std::string(pj.as_str());
                            blocks[cur_idx].accumulated_json += delta_text;
                        }
                    } else if (type_str == "thinking_delta") {
                        auto thinking = delta.get("thinking");
                        if (thinking.valid()) {
                            delta_text = std::string(thinking.as_str());
                            blocks[cur_idx].accumulated_text += delta_text;
                        }
                    }
                    if (!delta_text.empty() && options.on_event) {
                        ContentBlockDelta ev;
                        ev.index = cur_idx;
                        ev.delta_text = delta_text;
                        (*options.on_event)(ev);
                    }
                }
            }
        } else if (event_type == "content_block_stop") {
            auto idx = root.get("index");
            auto cur_idx = idx.valid() ? static_cast<std::uint32_t>(idx.as_int()) : block_index;
            // Finalize block content
            if (cur_idx < blocks.size()) {
                auto& accum = blocks[cur_idx];
                if (auto* tb = std::get_if<TextBlock>(&accum.block)) {
                    tb->text = std::move(accum.accumulated_text);
                } else if (auto* tub = std::get_if<ToolUseBlock>(&accum.block)) {
                    tub->input_json =
                        accum.accumulated_json.empty() ? "{}" : std::move(accum.accumulated_json);
                } else if (auto* thk = std::get_if<ThinkingBlock>(&accum.block)) {
                    thk->thinking = std::move(accum.accumulated_text);
                }
            }
            if (options.on_event) {
                ContentBlockStop ev;
                ev.index = cur_idx;
                (*options.on_event)(ev);
            }
        } else if (event_type == "message_delta") {
            auto delta = root.get("delta");
            if (delta.valid()) {
                auto sr = delta.get("stop_reason");
                if (sr.valid() && !sr.is_null()) {
                    result.message.stop_reason = std::string(sr.as_str());
                }
            }
            auto usage = root.get("usage");
            if (usage.valid()) {
                auto ot = usage.get("output_tokens");
                if (ot.valid())
                    result.usage.output_tokens = static_cast<std::uint32_t>(ot.as_int());
                auto cache_creation = usage.get("cache_creation_input_tokens");
                if (cache_creation.valid()) {
                    result.usage.cache_creation_tokens =
                        static_cast<std::uint32_t>(cache_creation.as_int());
                }
                auto cache_read = usage.get("cache_read_input_tokens");
                if (cache_read.valid()) {
                    result.usage.cache_read_tokens =
                        static_cast<std::uint32_t>(cache_read.as_int());
                }
            }
        } else if (event_type == "message_stop") {
            // Stream completed successfully
        } else if (event_type == "error") {
            auto err = root.get("error");
            if (err.valid()) {
                std::string type_text = "stream_error";
                std::string message_text = "Streaming API error";
                auto type = err.get("type");
                if (type.valid()) type_text = std::string(type.as_str());
                auto msg = err.get("message");
                if (msg.valid()) message_text = std::string(msg.as_str());
                const auto code = err.get("code");
                if (code.is_str()) message_text += " (" + std::string(code.as_str()) + ")";
                emit_stream_error(std::move(type_text), std::move(message_text));
            }
        }
    };

    // OpenAI tool fragments may interleave; the wire index identifies each
    // request-local accumulator independently from the UI block index.
    std::unordered_map<std::uint32_t, std::size_t> tool_blocks;
    std::optional<std::size_t> text_block;
    bool wire_started = false;
    bool wire_stopped = false;
    bool wire_terminated = false;
    bool wire_finish_reason = false;
    auto parse_wire_event = [&](const SseEvent& frame) {
        const auto first = frame.data.find_first_not_of(" \t\r\n");
        const auto last = frame.data.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && frame.data.substr(first, last - first + 1) == "[DONE]") {
            wire_terminated = true;
        }
        if (!wire_started) {
            wire_started = true;
            if (options.on_event) {
                StreamStart event;
                event.model = config_.model_params.model;
                (*options.on_event)(event);
            }
        }
        auto start_block = [&](ContentBlock block) {
            const auto index = blocks.size();
            blocks.push_back(BlockAccum{std::move(block), {}, {}});
            if (options.on_event) {
                ContentBlockStart event;
                event.index = static_cast<std::uint32_t>(index);
                event.block = blocks.back().block;
                (*options.on_event)(event);
            }
            return index;
        };
        auto emit_delta = [&](std::size_t index, const std::string& text) {
            if (options.on_event && !text.empty()) {
                ContentBlockDelta event;
                event.index = static_cast<std::uint32_t>(index);
                event.delta_text = text;
                (*options.on_event)(event);
            }
        };
        for (const auto& delta : backend->parse_stream_event(frame.type, frame.data)) {
            using Kind = loom::query::wire::StreamDelta::Kind;
            switch (delta.kind) {
            case Kind::TextDelta:
                if (!text_block) text_block = start_block(TextBlock{});
                blocks[*text_block].accumulated_text += delta.text;
                emit_delta(*text_block, delta.text);
                break;
            case Kind::ToolUseStart: {
                const auto key = delta.tool_index.value_or(0);
                auto found = tool_blocks.find(key);
                if (found == tool_blocks.end()) {
                    ToolUseBlock tool;
                    tool.id.value = delta.tool_id;
                    tool.name = delta.tool_name;
                    tool_blocks.emplace(key, start_block(std::move(tool)));
                } else {
                    auto& tool = std::get<ToolUseBlock>(blocks[found->second].block);
                    if ((!delta.tool_id.empty() && !tool.id.value.empty() &&
                         tool.id.value != delta.tool_id) ||
                        (!delta.tool_name.empty() && !tool.name.empty() &&
                         tool.name != delta.tool_name)) {
                        emit_stream_error("invalid_response",
                                          "Tool metadata changed within one index");
                        break;
                    }
                    if (!delta.tool_id.empty()) tool.id.value = delta.tool_id;
                    if (!delta.tool_name.empty()) tool.name = delta.tool_name;
                }
                break;
            }
            case Kind::ToolUseInputDelta: {
                const auto found = tool_blocks.find(delta.tool_index.value_or(0));
                if (found == tool_blocks.end()) {
                    emit_stream_error("invalid_response",
                                      "Tool arguments arrived before tool metadata");
                    break;
                }
                blocks[found->second].accumulated_json += delta.partial_json;
                emit_delta(found->second, delta.partial_json);
                break;
            }
            case Kind::Usage:
                result.usage = delta.usage;
                break;
            case Kind::MessageStop:
                if (!delta.stop_reason.empty()) {
                    result.message.stop_reason = delta.stop_reason;
                    wire_finish_reason = true;
                }
                if (!wire_stopped) {
                    wire_stopped = true;
                    for (std::size_t i = 0; i < blocks.size(); ++i) {
                        auto& accum = blocks[i];
                        if (auto* text = std::get_if<TextBlock>(&accum.block)) {
                            text->text = accum.accumulated_text;
                        } else if (auto* tool = std::get_if<ToolUseBlock>(&accum.block)) {
                            tool->input_json =
                                accum.accumulated_json.empty() ? "{}" : accum.accumulated_json;
                        }
                        if (options.on_event) {
                            ContentBlockStop event;
                            event.index = static_cast<std::uint32_t>(i);
                            (*options.on_event)(event);
                        }
                    }
                }
                break;
            case Kind::Error:
                emit_stream_error("stream_error", delta.text);
                break;
            default:
                break;
            }
        }
    };

    // Track current SSE event type
    // (now held inside sse_decoder_)

    // Streaming POST using httplib's send() with content_receiver on Request
    httplib::Request req;
    req.method = "POST";
    req.path = endpoint->path;
    req.headers = headers;
    req.headers.emplace("Content-Type", "application/json");
    req.body = body;
    bool accepted_response = false;
    std::string rejected_body;
    req.response_handler = [&](const httplib::Response& response) {
        accepted_response = response.status < 400;
        return true;
    };
    req.content_receiver = [&](const char* data, size_t len, uint64_t /*offset*/,
                               uint64_t /*total*/) -> bool {
        if (should_abort()) return false;
        if (!accepted_response) {
            const auto remaining = std::size_t{65536} - rejected_body.size();
            rejected_body.append(data, std::min(len, remaining));
            return true;
        }

        // The decoder buffers partial chunks and yields complete events;
        // dispatch each to the domain handler (parse_sse_event).
        for (const auto& ev : sse_decoder_.feed(std::string_view(data, len))) {
            if (backend->api() == loom::query::wire::WireApi::Messages) {
                parse_sse_event(ev.type, ev.data);
            } else {
                parse_wire_event(ev);
            }
        }

        return true;
    };

    httplib::Response res;
    httplib::Error err;
    // Register socket-shutdown callback so abort() (ESC) can interrupt
    // the blocking send() immediately instead of waiting for the read
    // timeout (up to 120 s).
    set_abort_callback([cli]() { cli->stop(); });
    if (should_abort()) {
        set_abort_callback(nullptr);
        result.failed = true;
        return result;
    }
    bool ok = cli->send(req, res, err);
    set_abort_callback(nullptr);

    // A user cancellation is complete once the interrupted send returns.
    // Do not restart that request through the non-streaming fallback.
    if (should_abort()) {
        result.failed = true;
        return result;
    }

    if (!ok || res.status >= 400) {
        const auto context_failure = loom::services::compact::classify_context_failure(
            res.status, rejected_body.empty() ? res.body : rejected_body);
        if (context_failure != loom::services::compact::ContextFailure::None) {
            if (!is_retry_after_compact) {
                auto compacted = compact_context_impl(
                    "reactive", options,
                    context_failure == loom::services::compact::ContextFailure::RequestBytesExceeded
                        ? body.size()
                        : 0,
                    true);
                if (compacted) return stream_single_api_call(options, true);
                emit_stream_error("context_compaction_error", compacted.error().message);
            } else {
                emit_stream_error("invalid_request_error",
                                  "Context still exceeded after one compaction retry");
            }
            if (options.on_event)
                (*options.on_event)(StreamError{result.error_type, result.error_message});
            return result;
        }
        // If streaming failed and we got nothing, fall back to non-streaming
        auto fallback = send_request(options, is_retry_after_compact);
        if (should_abort()) {
            result.failed = true;
            return result;
        }
        if (fallback) {
            result.message = std::move(fallback->message);
            result.usage = fallback->usage;
        } else {
            std::string type = "api_error";
            switch (fallback.error().code) {
            case ErrorCode::AuthenticationFailed:
                type = "authentication_error";
                break;
            case ErrorCode::InvalidRequest:
            case ErrorCode::ContextWindowExceeded:
                type = "invalid_request_error";
                break;
            case ErrorCode::RateLimited:
                type = "rate_limit_error";
                break;
            case ErrorCode::OverloadedError:
                type = "overloaded_error";
                break;
            case ErrorCode::ConnectionFailed:
            case ErrorCode::NetworkTimeout:
            case ErrorCode::SSLError:
                type = "network_error";
                break;
            default:
                break;
            }
            emit_stream_error(std::move(type), fallback.error().format());
            if (options.on_event)
                (*options.on_event)(StreamError{result.error_type, result.error_message});
            return result;
        }
    } else if (result.failed) {
        const auto category = loom::services::compact::classify_context_failure(
            0, result.error_type + " " + result.error_message);
        if (!is_retry_after_compact && category != loom::services::compact::ContextFailure::None) {
            auto compacted = compact_context_impl(
                "reactive", options,
                category == loom::services::compact::ContextFailure::RequestBytesExceeded
                    ? body.size()
                    : 0,
                true);
            if (compacted) return stream_single_api_call(options, true);
            emit_stream_error("context_compaction_error", compacted.error().message);
        }
        if (options.on_event)
            (*options.on_event)(StreamError{result.error_type, result.error_message});
        return result;
    } else {
        // Assemble final message from accumulated blocks
        result.message.timestamp = std::chrono::system_clock::now();
        for (auto& accum : blocks) {
            result.message.content.push_back(std::move(accum.block));
        }
        // Parse input_tokens from response if available
        auto usage_hdr = res.get_header_value("x-usage-input-tokens");
        if (!usage_hdr.empty()) {
            try {
                result.usage.input_tokens = static_cast<std::uint32_t>(std::stoul(usage_hdr));
            } catch (...) {
            }
        }
    }

    if (backend->api() != loom::query::wire::WireApi::Messages &&
        (!wire_stopped || !wire_terminated || !wire_finish_reason) && !result.failed && ok &&
        res.status < 400) {
        emit_stream_error("stream_error", "Model stream ended without a completion event");
    }

    if (backend->api() != loom::query::wire::WireApi::Messages && !result.failed) {
        for (const auto& block : result.message.content) {
            const auto* tool = std::get_if<ToolUseBlock>(&block);
            if (!tool) continue;
            const auto arguments = loom::utils::json::parse(tool->input_json);
            if (tool->id.value.empty() || tool->name.empty() || !arguments ||
                !arguments->root().is_obj()) {
                emit_stream_error("invalid_response",
                                  "Model stream ended with an incomplete or invalid tool call");
                break;
            }
        }
    }

    // Check for tool use
    for (const auto& block : result.message.content) {
        if (std::holds_alternative<ToolUseBlock>(block)) {
            result.has_tool_use = true;
            break;
        }
    }

    // Emit stream end
    if (result.failed && options.on_event) {
        (*options.on_event)(StreamError{result.error_type, result.error_message});
    }
    if (!result.failed && ok && res.status < 400)
        observe_context_usage(result.usage, sent_input, sent_schema);
    if (!result.failed && options.on_event) {
        StreamEnd end_event;
        end_event.stop_reason = result.message.stop_reason;
        end_event.usage = result.usage;
        (*options.on_event)(end_event);
    }

    // ── Dump Prompts: log API response ──
    if (dump_prompts_dir_) {
        auto dp = dump_prompts_path();
        if (dp) {
            std::ofstream ofs(*dp, std::ios::app);
            if (ofs.is_open()) {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
                // Serialize the assembled response message as JSONL
                std::string resp_json = message_to_jsonl_(Message{result.message});
                ofs << "{\"type\":\"response\",\"timestamp\":" << now_ms << ",\"stop_reason\":\""
                    << result.message.stop_reason.value_or("") << "\""
                    << ",\"has_tool_use\":" << (result.has_tool_use ? "true" : "false")
                    << ",\"failed\":" << (result.failed ? "true" : "false")
                    << ",\"message\":" << resp_json << "}\n";
            }
        }
    }

    return result;
}

}  // namespace loom::core
