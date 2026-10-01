// Implementation unit for loom.sdk.harness — the opaque embedding entrypoint
// (RFC 0001 cc-sdk phase 3, §2.2). Bodies live here (not the interface) so
// the engine/assembly/config/session imports never enter the module BMI:
// the interface names only the public DTOs, keeping the harness BMI light
// and the import list free of dead imports (graph_check).
module;

#include <cstdlib>

module loom.sdk.harness;

import std;

import loom.types.types;
import loom.config.config;        // ConfigManager, Settings
import loom.query.query_engine;  // QueryEngine, QueryOptions, ModelParams
import loom.query.assembly;     // assemble, resolve_engine_config, AssemblyConfig/Overrides/Callbacks
import loom.hooks.tool_permissions;  // AskUserResponseFn
import loom.session.storage;    // load_messages (resume path)
import loom.serdes.json;        // JsonVal (parse_session_message_value)

namespace loom::sdk {

namespace {

namespace fs = std::filesystem;

} // namespace

// ============================================================
// Harness::Impl — declaration order is load-bearing (§2.3)
// ============================================================

struct Harness::Impl {
    // (1) The HarnessConfig callbacks — declared FIRST, constructed first,
    // destroyed LAST. The AssemblyHandle's hook/registry reference these
    // (the hook's ask_user_response_fn wraps permission_callback_; the
    // registry's permission_check lambda captures the hook), so they must
    // outlive the handle. Reverse declaration order guarantees reverse
    // destruction: handle_ dies before the callbacks.
    PermissionCallback permission_callback_;
    EventSink event_sink_;
    BackendFactory backend_factory_;
    ApiKeyProvider api_key_provider_;

    // (2) The assembled engine (hook + registry + engine, in that order
    // inside AssemblyHandle::Impl). optional because assemble() can fail —
    // the error is stored in construction_error_ and surfaced by run()/
    // stream()/resume().
    std::optional<loom::query::AssemblyHandle> handle_;

    // (3) The abort flag, shared with the engine's external abort callback
    // (AssemblyConfig::cancel_flag). abort() stores true; run()/stream()
    // entry exchanges it (§2.3).
    std::shared_ptr<std::atomic_bool> abort_requested_;

    // Construction-path state (no lifetime concerns).
    std::optional<fs::path> sessions_dir_;
    std::optional<loom::core::Error> construction_error_;
};

// ============================================================
// Construction
// ============================================================

Harness::Harness(HarnessConfig config) : impl_(std::make_unique<Impl>()) {
    // Move the callbacks out first (they are the declared-first members).
    impl_->permission_callback_ = std::move(config.permission_callback);
    impl_->event_sink_ = std::move(config.event_sink);
    impl_->backend_factory_ = std::move(config.backend_factory);
    impl_->api_key_provider_ = std::move(config.api_key_provider);
    impl_->sessions_dir_ = config.sessions_dir;
    impl_->abort_requested_ = std::make_shared<std::atomic_bool>(false);

    // Load ConfigManager as a construction-path local (§2.3): load ->
    // settings() -> resolve_engine_config -> discard. It is unused after
    // the config is resolved, so it is NOT an owned member.
    loom::core::ConfigManager manager;
    if (auto loaded = manager.load(); !loaded) {
        impl_->construction_error_ = loaded.error();
        return;
    }
    const auto& settings = manager.settings();

    // Resolve the api_key (§2.2): provider first, else settings, else a
    // placeholder when base_url is set (a loopback/gateway ignores it, so
    // a mock/loopback harness is never blocked by an empty key). The
    // resolver itself does NOT hard-error on an empty key — the caller
    // enforces its own key policy (server_routes.cppm:733-735).
    std::optional<std::string> api_key;
    if (impl_->api_key_provider_) {
        api_key = impl_->api_key_provider_();
    } else if (settings.network.api_key && !settings.network.api_key->empty()) {
        // Let the resolver pick up settings.network.api_key (do not override).
    } else if (config.base_url) {
        api_key = "sk-loom-placeholder";
    }

    // Build AssemblyOverrides from HarnessConfig — the shared resolver, not
    // a second settings->config implementation (§2.2).
    loom::query::AssemblyOverrides overrides;
    if (!config.model.empty()) overrides.requested_model = config.model;
    overrides.api_key = api_key;
    overrides.base_url = config.base_url;
    overrides.wire_api = config.wire_api;
    if (!config.cwd.empty()) overrides.cwd = config.cwd.string();

    auto resolved = loom::query::resolve_engine_config(settings, overrides);
    if (!resolved) {
        impl_->construction_error_ = resolved.error();
        return;
    }
    auto engine_config = std::move(*resolved);

    // Construction-time-only settings (no per-turn engine seam — §2.2
    // field mapping). Varying these requires a new Harness.
    if (config.max_budget_usd) engine_config.max_budget_usd = *config.max_budget_usd;
    if (config.max_turns) engine_config.max_turns = *config.max_turns;
    if (config.system_prompt) engine_config.custom_system_prompt = *config.system_prompt;
    if (config.append_system_prompt) engine_config.append_system_prompt = *config.append_system_prompt;
    // The resolver already set always_deny_rules from settings; append the
    // embedder's rules (both sources apply).
    for (auto& rule : config.always_deny_rules) {
        engine_config.always_deny_rules.push_back(std::move(rule));
    }

    // Build AssemblyConfig.
    loom::query::AssemblyConfig ac;
    ac.engine = std::move(engine_config);
    ac.sessions_dir = config.sessions_dir;
    ac.dump_prompts_dir = config.dump_prompts_dir;
    ac.cancel_flag = impl_->abort_requested_;
    ac.register_extra_tools = std::move(config.register_extra_tools);

    // Build AssemblyCallbacks. When a PermissionCallback is set, bridge it
    // as the hook's ask_user response fn (the assembly wires the hook ->
    // registry permission_check bridge and engine.set_permission_hook when
    // ask_user is present).
    loom::query::AssemblyCallbacks callbacks;
    if (impl_->permission_callback_) {
        callbacks.ask_user = loom::hooks::AskUserResponseFn{impl_->permission_callback_};
    }

    auto assembled = loom::query::assemble(ac, callbacks);
    if (!assembled) {
        impl_->construction_error_ = assembled.error();
        return;
    }
    impl_->handle_ = std::move(*assembled);

    // Wire the WireBackend factory seam (§2.4) — same-module setter, no
    // graph effect. Body-serialization only; the transport is not
    // intercepted.
    if (impl_->backend_factory_) {
        impl_->handle_->engine().set_wire_backend_factory(impl_->backend_factory_);
    }
}

Harness::~Harness() = default;
Harness::Harness(Harness&&) noexcept = default;
Harness& Harness::operator=(Harness&&) noexcept = default;

// ============================================================
// run / stream
// ============================================================

loom::core::Result<TurnResult> Harness::run(const TurnOptions& options) {
    if (impl_->construction_error_) {
        return std::unexpected(*impl_->construction_error_);
    }
    // Abort lifecycle (§2.3): exchange (not load) so the error returns once
    // for a pre-turn abort, then clears so the next turn proceeds. The
    // engine's own aborted_ flag is auto-reset at query() entry, so this
    // harness-level flag is what makes "abort before run" observable.
    if (impl_->abort_requested_->exchange(false)) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InternalError, "Query interrupted"));
    }

    auto& engine = impl_->handle_->engine();

    // Per-turn model override (§2.2 mapping: model -> set_model_params).
    if (options.model) {
        auto params = engine.model_params();
        params.model = *options.model;
        engine.set_model_params(params);
    }

    loom::core::QueryOptions qo;
    if (options.allowed_tools) qo.enabled_tools = *options.allowed_tools;
    qo.attachments = options.attachments;
    if (impl_->event_sink_) {
        qo.on_event = impl_->event_sink_;
    }

    auto response = engine.query(options.prompt, qo);
    if (!response) {
        return std::unexpected(response.error());
    }

    TurnResult result;
    result.message = response->message;
    result.usage = response->total_usage;
    result.tool_rounds = response->tool_rounds;
    result.elapsed = response->elapsed;
    result.budget_exceeded = response->budget_exceeded;
    result.success = response->success;
    result.errors = response->errors;
    return result;
}

void Harness::stream(const TurnOptions& options, const EventSink& sink) {
    auto report_error = [&](const loom::core::Error& err) {
        if (sink) {
            loom::core::StreamError ev;
            ev.error_type = "harness_error";
            ev.message = err.format();
            sink(loom::core::StreamEvent{std::move(ev)});
        }
    };

    if (impl_->construction_error_) {
        report_error(*impl_->construction_error_);
        return;
    }
    // Same exchange-at-entry semantics as run() (§2.3).
    if (impl_->abort_requested_->exchange(false)) {
        if (sink) {
            loom::core::StreamError ev;
            ev.error_type = "harness_error";
            ev.message = "Query interrupted";
            sink(loom::core::StreamEvent{std::move(ev)});
        }
        return;
    }

    auto& engine = impl_->handle_->engine();

    if (options.model) {
        auto params = engine.model_params();
        params.model = *options.model;
        engine.set_model_params(params);
    }

    loom::core::QueryOptions qo;
    if (options.allowed_tools) qo.enabled_tools = *options.allowed_tools;
    qo.attachments = options.attachments;
    qo.on_event = sink;

    engine.stream_query(options.prompt, qo);
}

// ============================================================
// abort / resume / accessors
// ============================================================

void Harness::abort() noexcept {
    impl_->abort_requested_->store(true);
    if (impl_->handle_) {
        impl_->handle_->engine().abort();
    }
}

loom::core::Result<void> Harness::resume(std::string_view session_id) {
    if (impl_->construction_error_) {
        return std::unexpected(*impl_->construction_error_);
    }
    if (!impl_->sessions_dir_) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::InvalidInput,
            "resume() requires sessions_dir in HarnessConfig"));
    }

    // Real engine resume path (§2.3): load_messages (rank 6) ->
    // parse_session_message_value (the assembly's role/content-string
    // reader — lossy for tool_use/tool_result/image blocks; see §2.3
    // deviation note) -> restore_conversation (which also rebuilds
    // content-replacement state). No loom.server import. (OpenSSL DOES
    // enter the SDK closure via loom_query -> loom_services — the accepted
    // phase-3 cost; see §3.4.)
    auto docs = loom::session::load_messages(*impl_->sessions_dir_, session_id);
    if (docs.empty()) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::SessionNotFound,
            std::format("No messages found for session {}", session_id)));
    }

    std::vector<loom::core::Message> messages;
    messages.reserve(docs.size());
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (auto msg = loom::query::parse_session_message_value(docs[i].root(), i)) {
            messages.push_back(std::move(*msg));
        }
    }
    if (messages.empty()) {
        return std::unexpected(loom::core::Error::make(
            loom::core::ErrorCode::SessionCorrupted,
            std::format("No parseable messages for session {}", session_id)));
    }

    impl_->handle_->engine().restore_conversation(std::move(messages));
    return {};
}

std::string Harness::session_id() const {
    if (!impl_->handle_) return {};
    return impl_->handle_->engine().session_id().str();
}

std::vector<loom::core::Message> Harness::conversation() const {
    if (!impl_->handle_) return {};
    return impl_->handle_->engine().get_conversation();
}

} // namespace loom::sdk
