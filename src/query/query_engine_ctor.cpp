// Implementation unit for loom.query.query_engine — the QueryEngine
// constructor and API-client setup. This is the ONLY implementation unit
// that imports loom.services.analytics (the local-only session_start event),
// so that dependency never enters the module interface BMI.
module;

#include <cstdlib>

module loom.query.query_engine;

import std;

import loom.query.wire_protocol;
import loom.services.analytics;
import loom.services.compact.model_context;

namespace loom::core {

std::shared_ptr<const ApiExecutionContext>
make_api_execution_context(const QueryEngineConfig& config) {
    auto context = std::make_shared<ApiExecutionContext>();
    context->base_url = config.base_url.value_or("");
    context->api_key = config.api_key;
    context->auth_token = config.auth_token;
    context->context_output_tokens = config.model_params.max_tokens;
    context->context_compaction_threshold = config.context_window.compaction_threshold;
    context->context_auto_compact = config.context_window.auto_compact;
    const auto selected = config.wire_api.value_or(
        std::getenv("LOOM_WIRE_API") ? std::getenv("LOOM_WIRE_API") : "messages");
    if (auto api = loom::query::wire::wire_api_from_string(selected)) {
        context->wire_api = std::string(loom::query::wire::wire_api_name(*api));
    }
    context->model_context_config = std::make_shared<const ModelContextConfig>(
        ModelContextConfig{config.context_window.max_context_tokens, config.model_context_limits});
    context->context_window_tokens = loom::services::compact::resolve_model_context(
        *context->model_context_config, config.model_params.model, context->base_url,
        context->wire_api, config.model_params.max_tokens).window_tokens;
    return context;
}

std::shared_ptr<const ApiExecutionContext> QueryEngine::api_execution_context() const {
    auto context = std::make_shared<ApiExecutionContext>();
    context->wire_api = std::string(loom::query::wire::wire_api_name(wire_api_));
    context->base_url = api_config_.base_url;
    context->api_key = api_config_.api_key;
    context->auth_token = api_config_.auth_token;
    context->api_version = api_config_.api_version;
    context->timeout = api_config_.timeout;
    context->context_window_tokens = model_context_.window_tokens;
    context->context_output_tokens = config_.model_params.max_tokens;
    context->model_context_config = model_context_config_;
    context->context_compaction_threshold = config_.context_window.compaction_threshold;
    context->context_auto_compact = config_.context_window.auto_compact;
    return context;
}

QueryEngine::QueryEngine(QueryEngineConfig config, ToolRegistry& registry)
    : config_(std::move(config)), tool_registry_(&registry),
      session_start_(std::chrono::system_clock::now()) {
    // Initialize session ID
    if (config_.session_id_override &&
        !config_.session_id_override->empty()) {
        session_id_.value = *config_.session_id_override;
    } else {
        session_id_.value = generate_session_id();
    }

    // Set up budget
    if (config_.task_budget) {
        configured_task_budget_remaining_ = config_.task_budget->remaining;
    }
    if (config_.max_budget_usd) {
        budget_tracker_.max_budget_usd = *config_.max_budget_usd;
    }

    // Initialize API client config
    setup_api_client();
    model_context_config_ = std::make_shared<const ModelContextConfig>(
        ModelContextConfig{config_.context_window.max_context_tokens, config_.model_context_limits});
    refresh_model_context();

    // Initialize conversation with system prompt if available
    build_and_add_system_prompt();
    initialize_context_schema();

    // Record the session start in the local-only event log. The engine is
    // the one place every entry point (REPL, server, headless) passes
    // through, so emitting here means the log is actually populated rather
    // than being a writer nobody calls -- which is how the previous
    // analytics module ended up dead.
    loom::services::analytics::local_analytics().log_event(
        "session_start",
        {{"wire_api", std::string(loom::query::wire::wire_api_name(wire_api_))}});
}

void QueryEngine::refresh_model_context() {
    model_context_ = loom::services::compact::resolve_model_context(
        *model_context_config_, config_.model_params.model, api_config_.base_url,
        loom::query::wire::wire_api_name(wire_api_), config_.model_params.max_tokens);
}

void QueryEngine::set_model_params(ModelParams params) {
    std::lock_guard lock(conversation_mutex_);
    config_.model_params = std::move(params);
    refresh_model_context();
    reset_context_observation_locked();
    ++conversation_revision_;
}

void QueryEngine::setup_api_client() {
    api_config_.base_url = config_.base_url.value_or("");
    api_config_.api_key = config_.api_key;
    api_config_.auth_token = config_.auth_token;
    api_config_.api_version = "2023-06-01";
    api_config_.timeout = std::chrono::milliseconds{120000};
    api_config_.max_retries = static_cast<int>(config_.retry_policy.max_retries);
    api_config_.base_retry_delay = config_.retry_policy.initial_delay;

    // Select the wire protocol. Unset keeps the historical behaviour
    // (Messages API /v1/messages). See loom.query.wire_protocol for the seam.
    wire_api_ = loom::query::wire::WireApi::Messages;
    if (config_.wire_api && !config_.wire_api->empty()) {
        if (auto parsed = loom::query::wire::wire_api_from_string(*config_.wire_api)) {
            wire_api_ = *parsed;
        }
    } else {
        const char* env = std::getenv("LOOM_WIRE_API");
        if (env && *env) {
            if (auto parsed = loom::query::wire::wire_api_from_string(env)) {
                wire_api_ = *parsed;
            }
        }
    }
    native_computer_tool_ =
        config_.native_computer_tool.value_or(wire_api_ == loom::query::wire::WireApi::Messages);
}

} // namespace loom::core
