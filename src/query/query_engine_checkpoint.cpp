// RFC 0025: owner-local checkpoint commit/restore and shared-policy integration.
module loom.query.query_engine;

import std;
import loom.services.compact.context_compaction;
import loom.session.history;
import loom.session.storage;
import loom.fs.atomic_replace;
import loom.query.wire_protocol;
import loom.serdes.json;

namespace loom::core {
namespace {
namespace compact = loom::services::compact;

std::string observation_key(const loom::query::wire::RequestInput& input,
                            std::string_view schema,
                            loom::query::wire::WireApi api) {
    return std::string(loom::query::wire::wire_api_name(api)) + "\n" +
        input.model + "\n" + input.system_prompt + "\n" + std::string(schema);
}

std::optional<std::size_t> request_anchor(const std::vector<Message>& messages) {
    for (std::size_t index = messages.size(); index > 0; --index) {
        const auto* user = std::get_if<UserMessage>(&messages[index - 1]);
        if (!user || std::holds_alternative<TaskEvent>(user->origin)) continue;
        const bool only_results = !user->content.empty() &&
            std::ranges::all_of(user->content, [](const auto& block) {
                return std::holds_alternative<ToolResultBlock>(block);
            });
        if (only_results) continue;
        return index - 1;
    }
    return {};
}

bool append_context_record(const std::filesystem::path& path, std::string_view record) {
    std::string previous;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path, ec)) || ec) return false;
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        previous.assign(std::istreambuf_iterator<char>(in), {});
        if (!in.eof() && in.fail()) return false;
    } else if (ec) return false;
    if (!previous.empty() && previous.back() != '\n') previous += '\n';
    previous.append(record);
    previous += '\n';
    return loom::utils::atomic_replace_file(path, previous).has_value();
}

} // namespace

ContextUsageSnapshot QueryEngine::context_usage() const {
    std::lock_guard lock(conversation_mutex_);
    if (!context_tokens_cache_ || context_tokens_cache_revision_ != conversation_revision_) {
        loom::query::wire::RequestInput input;
        input.model = config_.model_params.model;
        project_context_messages_locked(input);
        compact::UsageObservation observed;
        if (context_observed_key_ == observation_key(input, context_schema_snapshot_, wire_api_))
            observed = context_observed_;
        context_tokens_cache_ = compact::estimate_request_tokens({input.messages, input.system_prompt,
            context_schema_snapshot_, {}, observed});
        context_tokens_cache_revision_ = conversation_revision_;
    }
    return {*context_tokens_cache_, model_context_.window_tokens, last_request_usage_};
}

void QueryEngine::reset_context_observation_locked() {
    context_observed_ = {};
    context_observed_key_.clear();
    last_request_usage_.reset();
    context_tokens_cache_.reset();
}

void QueryEngine::capture_context_schema(std::string_view prepared_schema) {
    std::lock_guard lock(conversation_mutex_);
    context_schema_snapshot_ = prepared_schema;
    context_tokens_cache_.reset();
}

void QueryEngine::initialize_context_schema() {
    // Static metadata is known before sampling; dynamic providers belong only to real requests.
    auto input = build_wire_input(QueryOptions{}, false, false);
    input.messages.clear();
    input.system_prompt.clear();
    capture_context_schema(context_schema_text(input));
}

std::optional<std::size_t> QueryEngine::context_request_anchor_locked(
    const std::vector<Message>& messages) const {
    if (!context_request_anchor_id_) return {};
    for (std::size_t index = messages.size(); index > 0; --index) {
        if (const auto* user = std::get_if<UserMessage>(&messages[index - 1]);
            user && user->id.value == *context_request_anchor_id_) return index - 1;
    }
    return {};
}

void QueryEngine::restore_context_request_anchor_locked() {
    context_request_anchor_id_.reset();
    if (const auto anchor = request_anchor(conversation_)) {
        context_request_anchor_id_ = message_id_value(conversation_[*anchor]);
    }
}

std::string QueryEngine::context_schema_text(const loom::query::wire::RequestInput& input) const {
    auto backend = make_wire_backend();
    auto request = backend->prepare(input);
    if (!request) return {};
    return context_schema_text(std::string_view(request->body));
}

std::string QueryEngine::context_schema_text(std::string_view prepared_body) {
    const auto parsed = loom::utils::json::parse(prepared_body);
    if (!parsed) return {};
    std::string schema;
    for (const auto name : {"tools", "output_config", "response_format"}) {
        const auto field = parsed->root().get(name);
        if (field.valid()) schema += field.to_string();
    }
    return schema;
}

VoidResult QueryEngine::ensure_context_budget(const QueryOptions& options) {
    if (wire_api_ == loom::query::wire::WireApi::Messages && thinking_enabled_for_request() &&
        config_.thinking_config.mode == ThinkingConfig::Mode::Forced) {
        const auto thinking_budget = config_.thinking_config.budget_tokens.value_or(10000);
        if (thinking_budget >= model_context_.output_tokens) return std::unexpected(Error::make(
            ErrorCode::InvalidRequest, std::format(
                "Forced thinking budget {} must be below the effective model output limit {}; "
                "reduce the thinking budget or select a model with a larger output limit",
                thinking_budget, model_context_.output_tokens)));
    }
    if (!config_.context_window.auto_compact) return {};
    const auto input = build_wire_input(options, false);
    const auto schema = context_schema_text(input);
    const compact::ContextPolicy policy{model_context_.window_tokens,
        model_context_.output_tokens, config_.context_window.compaction_threshold};
    compact::UsageObservation observed;
    std::optional<std::size_t> anchor;
    {
        std::lock_guard lock(conversation_mutex_);
        if (context_observed_key_ != observation_key(input, schema, wire_api_)) {
            context_observed_ = {};
            context_observed_key_.clear();
            context_tokens_cache_.reset();
        }
        observed = context_observed_;
        anchor = context_request_anchor_locked(input.messages);
    }
    const compact::RequestContext context{input.messages, input.system_prompt, schema,
                                         anchor, observed};
    const auto ceiling = compact::usable_input_tokens(policy);
    if (ceiling == 0) return std::unexpected(Error::make(ErrorCode::ContextWindowExceeded,
        "Invalid context window or response reserve"));
    if (compact::estimate_request_tokens(context) < ceiling) return {};
    return compact_context_impl("auto", options);
}

void QueryEngine::observe_context_usage(const TokenUsage& usage,
    const loom::query::wire::RequestInput& sent_input, std::string_view sent_schema) {
    compact::RequestContext context{sent_input.messages, sent_input.system_prompt, sent_schema, {}, {}};
    std::uint64_t observed = usage.input_tokens;
    if (wire_api_ == loom::query::wire::WireApi::Messages) {
        observed += usage.cache_creation_tokens;
        observed += usage.cache_read_tokens;
    }
    std::lock_guard lock(conversation_mutex_);
    if (usage.input_tokens || usage.output_tokens || usage.cache_creation_tokens || usage.cache_read_tokens)
        last_request_usage_ = usage;
    else last_request_usage_.reset();
    context_tokens_cache_.reset();
    if (observed == 0) return;
    context_observed_ = {observed, compact::estimate_request_tokens(context, false)};
    context_observed_key_ = observation_key(sent_input, sent_schema, wire_api_);
}

VoidResult QueryEngine::compact_context_impl(std::string_view trigger, const QueryOptions& options,
    std::size_t rejected_payload_bytes, bool payload_stream) {
    std::uint64_t revision;
    {
        std::lock_guard lock(conversation_mutex_);
        if (std::ranges::all_of(conversation_, [](const auto& message) {
                return std::holds_alternative<SystemMessage>(message);
            })) return {};
        revision = conversation_revision_;
    }
    const auto input = build_wire_input(options, false);
    const auto schema = context_schema_text(input);
    compact::UsageObservation observed;
    std::optional<std::size_t> anchor;
    {
        std::lock_guard lock(conversation_mutex_);
        if (revision != conversation_revision_) return std::unexpected(Error::make(
            ErrorCode::InternalError, "Conversation changed while preparing compaction"));
        if (context_observed_key_ == observation_key(input, schema, wire_api_)) observed = context_observed_;
        anchor = context_request_anchor_locked(input.messages);
    }
    const compact::RequestContext context{input.messages, input.system_prompt, schema,
                                         anchor, observed};
    const compact::ContextPolicy policy{model_context_.window_tokens,
        model_context_.output_tokens, config_.context_window.compaction_threshold};
    auto compacted = compact::compact_context(context, policy,
        [this](const auto& request) { return summarize_context(request); },
        [this] { return should_abort() || budget_tracker_.budget_exceeded; });
    if (!compacted) return std::unexpected(compacted.error());
    const auto summary_id = generate_id();
    std::vector<Message> replacement;
    SystemMessage base{};
    base.id.value = generate_id();
    base.timestamp = std::chrono::system_clock::now();
    base.content.push_back(TextBlock{input.system_prompt});
    replacement.emplace_back(std::move(base));
    SystemMessage boundary{};
    boundary.id.value = generate_id();
    boundary.timestamp = std::chrono::system_clock::now();
    boundary.subtype = "compact_boundary";
    boundary.compact_metadata = CompactMetadata{
        .trigger = std::string(trigger),
        .pre_tokens = static_cast<std::uint32_t>(std::min<std::uint64_t>(compacted->before_tokens, std::numeric_limits<std::uint32_t>::max())),
        .preserved_segment = std::nullopt};
    boundary.content.push_back(TextBlock{std::format("Conversation compacted by {} compact.", trigger)});
    UserMessage marker{};
    marker.id.value = summary_id;
    marker.timestamp = std::chrono::system_clock::now();
    marker.content.push_back(TextBlock{compacted->summary});
    replacement.emplace_back(std::move(boundary));
    replacement.emplace_back(std::move(marker));
    for (const auto index : compacted->retained_indices) replacement.push_back(input.messages[index]);
    auto& boundary_value = std::get<SystemMessage>(replacement[1]);
    boundary_value.compact_metadata->preserved_segment = compact_preserved_segment(
        replacement.begin() + 3, replacement.end(), summary_id);
    if (rejected_payload_bytes != 0) {
        auto candidate = input;
        candidate.messages.clear();
        for (const auto& message : replacement) {
            if (!std::holds_alternative<SystemMessage>(message)) candidate.messages.push_back(message);
        }
        candidate.stream = payload_stream;
        auto prepared = make_wire_backend()->prepare(candidate);
        auto payload_bytes = prepared ? prepared->body.size() : rejected_payload_bytes;
        if (prepared && config_.task_budget && !config_.task_budget->remaining) {
            const auto parsed = loom::utils::json::parse(prepared->body);
            const auto budget = parsed ? parsed->root().get("output_config").get("task_budget")
                                       : loom::utils::json::JsonVal{};
            if (budget.is_obj() && !budget.get("remaining").valid()) {
                const auto total = config_.task_budget->total;
                const auto consumed = boundary_value.compact_metadata->pre_tokens;
                const auto remaining = consumed >= total ? 0 : total - consumed;
                payload_bytes += std::string_view("\"remaining\":").size() +
                    std::to_string(remaining).size() + (budget.size() != 0 ? 1 : 0);
            }
        }
        if (!prepared || payload_bytes >= rejected_payload_bytes) {
            return std::unexpected(Error::make(ErrorCode::ContextWindowExceeded,
                "Compaction did not reduce the rejected request payload; original history retained"));
        }
    }
    {
        std::lock_guard lock(conversation_mutex_);
        if (revision != conversation_revision_ || should_abort() || budget_tracker_.budget_exceeded) {
            return std::unexpected(Error::make(ErrorCode::InternalError,
                "Compaction cancelled or conversation changed; original history retained"));
        }
        std::optional<std::uint32_t> remaining;
        if (config_.task_budget) {
            const auto current = config_.task_budget->remaining.value_or(config_.task_budget->total);
            const auto consumed = boundary_value.compact_metadata->pre_tokens;
            remaining = consumed >= current ? 0 : current - consumed;
        }
        if (!persist_context_checkpoint_locked(replacement, remaining)) return std::unexpected(Error::make(
            ErrorCode::ConfigWriteError, "Unable to persist context checkpoint; original history retained"));
        update_task_budget_remaining_after_compact(boundary_value.compact_metadata->pre_tokens);
        conversation_ = std::move(replacement);
        ++conversation_revision_;
        reset_context_observation_locked();
    }
    append_session_summary(working_directory(), compacted->summary);
    return {};
}

void QueryEngine::persist_context_request_anchor_locked(std::string_view anchor_id) const {
    if (!sessions_dir_) return;
    const auto path = loom::session::get_session_dir(*sessions_dir_, session_id_.str()) /
        "context-checkpoints.jsonl";
    loom::utils::json::JsonMutDoc doc;
    auto root = doc.object();
    root.add("kind", doc.string("context_request_anchor"));
    root.add("version", doc.number(static_cast<std::int64_t>(1)));
    root.add("session_id", doc.string(session_id_.str()));
    root.add("request_anchor_id", doc.string(anchor_id));
    doc.set_root(root);
    // Ordinary user-message appends retain best-effort persistence semantics.
    // Restore validates this ID against the audit and falls back for old logs.
    (void)append_context_record(path, doc.to_string());
}

bool QueryEngine::persist_context_checkpoint_locked(const std::vector<Message>& candidate,
    std::optional<std::uint32_t> task_budget_remaining) const {
    if (!sessions_dir_) return true;
    const auto path = loom::session::get_session_dir(*sessions_dir_, session_id_.str()) /
        "context-checkpoints.jsonl";
    std::string audit_anchor;
    for (auto it = conversation_.rbegin(); it != conversation_.rend(); ++it) {
        if (!std::holds_alternative<SystemMessage>(*it)) { audit_anchor = message_id_value(*it); break; }
    }
    if (audit_anchor.empty()) return false;
    const auto audit_path = loom::session::get_messages_path(*sessions_dir_, session_id_.str());
    std::error_code audit_error;
    if (!std::filesystem::is_regular_file(audit_path, audit_error) || audit_error) return false;
    const auto audit = loom::session::load_messages(*sessions_dir_, session_id_.str());
    bool persisted_anchor = false;
    for (std::size_t index = 0; index < audit.size(); ++index) {
        const auto id = audit[index].root().get("id");
        if ((id.is_str() && id.as_str() == audit_anchor) ||
            (!id.is_str() && audit_anchor == "restored_" + std::to_string(index))) {
            persisted_anchor = true;
            break;
        }
    }
    if (!persisted_anchor) return false;
    loom::utils::json::JsonMutDoc doc;
    auto root = doc.object();
    root.add("kind", doc.string("context_checkpoint"));
    root.add("version", doc.number(static_cast<std::int64_t>(1)));
    root.add("session_id", doc.string(session_id_.str()));
    root.add("audit_anchor_id", doc.string(audit_anchor));
    if (context_request_anchor_id_) {
        root.add("request_anchor_id", doc.string(*context_request_anchor_id_));
    }
    if (config_.task_budget && task_budget_remaining) {
        root.add("task_budget_total", doc.number(static_cast<std::int64_t>(config_.task_budget->total)));
        root.add("task_budget_remaining", doc.number(static_cast<std::int64_t>(*task_budget_remaining)));
    }
    auto messages = doc.array();
    for (const auto& message : candidate) messages.append(ConversationStore::serialize_message(doc, message));
    root.add("working_messages", messages);
    doc.set_root(root);
    return append_context_record(path, doc.to_string());
}

void QueryEngine::restore_context_checkpoint_locked() {
    if (!sessions_dir_) return;
    const auto path = loom::session::get_session_dir(*sessions_dir_, session_id_.str()) /
        "context-checkpoints.jsonl";
    std::ifstream input(path);
    if (!input) return;
    std::vector<Message> restored;
    std::size_t after_anchor = 0;
    std::optional<std::string> restored_request_anchor;
    std::optional<std::string> latest_owner_anchor;
    std::optional<std::uint32_t> restored_remaining;
    for (std::string line; std::getline(input, line);) {
        const auto parsed = loom::utils::json::parse(line);
        if (!parsed) continue;
        const auto root = parsed->root();
        if (root.get_int("version") != 1 || root.get_string("session_id") != session_id_.str()) continue;
        if (root.get_string("kind") == "context_request_anchor") {
            const auto value = root.get("request_anchor_id");
            if (!value.is_str()) continue;
            const auto found = std::ranges::find_if(conversation_, [&](const auto& message) {
                const auto* user = std::get_if<UserMessage>(&message);
                return user && !std::holds_alternative<TaskEvent>(user->origin) &&
                    user->id.value == value.as_str();
            });
            if (found != conversation_.end()) latest_owner_anchor = std::string(value.as_str());
            continue;
        }
        if (root.get_string("kind") != "context_checkpoint") continue;
        const auto anchor_id = root.get_string("audit_anchor_id");
        const auto anchor = std::ranges::find_if(conversation_, [&](const auto& message) {
            return message_id_value(message) == anchor_id;
        });
        if (anchor == conversation_.end()) continue;
        const auto values = root.get("working_messages");
        if (!values.is_arr() || values.size() == 0) continue;
        std::vector<Message> next;
        bool valid = true;
        values.iter([&](const auto value) {
            const auto message = ConversationStore::parse_message(value);
            if (!message) valid = false;
            else next.push_back(*message);
        });
        if (!valid || next.size() < 3 || !std::holds_alternative<SystemMessage>(next.front()) ||
            !compact::valid_tool_history(next)) continue;
        const auto* boundary = std::get_if<SystemMessage>(&next[1]);
        const auto* summary = std::get_if<UserMessage>(&next[2]);
        if (!boundary || boundary->subtype != "compact_boundary" || !summary || summary->content.empty()) continue;
        const auto* text = std::get_if<TextBlock>(&summary->content.front());
        if (!text || !compact::valid_checkpoint(text->text)) continue;
        std::optional<std::uint32_t> remaining;
        if (const auto value = root.get("task_budget_remaining"); value.valid()) {
            const auto total = root.get("task_budget_total");
            if (!value.is_int() || value.as_int() < 0 || !total.is_int() ||
                total.as_int() < value.as_int() ||
                total.as_int() > std::numeric_limits<std::uint32_t>::max()) continue;
            if (config_.task_budget && total.as_int() == config_.task_budget->total) {
                remaining = static_cast<std::uint32_t>(value.as_int());
            }
        }
        std::optional<std::string> human_anchor;
        if (const auto value = root.get("request_anchor_id"); value.valid()) {
            if (!value.is_str()) continue;
            const auto found = std::ranges::find_if(next, [&](const auto& message) {
                const auto* user = std::get_if<UserMessage>(&message);
                return user && !std::holds_alternative<TaskEvent>(user->origin) &&
                    user->id.value == value.as_str();
            });
            if (found == next.end()) continue;
            human_anchor = std::string(value.as_str());
        }
        restored = std::move(next);
        after_anchor = static_cast<std::size_t>(anchor - conversation_.begin()) + 1;
        restored_request_anchor = std::move(human_anchor);
        if (restored_request_anchor) latest_owner_anchor = restored_request_anchor;
        restored_remaining = remaining;
    }
    if (!restored.empty()) {
        restored.insert(restored.end(), conversation_.begin() + static_cast<std::ptrdiff_t>(after_anchor), conversation_.end());
        conversation_ = std::move(restored);
        if (restored_request_anchor) context_request_anchor_id_ = std::move(restored_request_anchor);
        else restore_context_request_anchor_locked();
        if (config_.task_budget && !configured_task_budget_remaining_ && restored_remaining) {
            config_.task_budget->remaining = restored_remaining;
        }
    }
    if (latest_owner_anchor && std::ranges::any_of(conversation_, [&](const auto& message) {
            return message_id_value(message) == *latest_owner_anchor;
        })) context_request_anchor_id_ = std::move(latest_owner_anchor);
}

} // namespace loom::core
