/// @file harness.cppm
/// @brief loom.sdk.harness — the opaque embedding entrypoint (RFC 0001
///        cc-sdk phase 3, §2.2).
///
/// The Harness wraps the extracted engine recipe (loom.query.assembly,
/// rank 10) behind an opaque PIMPL handle: construct once, run/stream many
/// turns, abort, resume. The embedder holds one Harness by value (or
/// unique_ptr); one Harness = one session (the owned QueryEngine holds the
/// multi-turn conversation). For concurrent sessions, construct multiple
/// Harness instances.
///
/// Layering (§3.4): this module sits at rank 16 (the apex) and imports only
/// downward — loom.types (0), loom.hooks (4), loom.tools (8), loom.query.wire_protocol
/// (10). It does NOT import loom.server (rank 13), so OpenSSL::Crypto (a
/// loom_server dep) never enters the SDK link closure. The assembly import
/// lives in the implementation unit (harness.cpp), not here — the interface
/// names no assembly type, and an unused interface import would be a dead
/// import (graph_check).
///
/// Ownership and lifetime (§2.3): Harness::Impl declares the HarnessConfig
/// callbacks FIRST (constructed first, destroyed LAST), then the
/// AssemblyHandle, then the abort flag. Reverse declaration order guarantees
/// reverse destruction: the handle (and its hook/registry/engine) dies before
/// the callbacks it references. See harness.cpp for the construction body.
module;

#include <cstdint>

export module loom.sdk.harness;

import std;

import loom.types.types;            // ContentBlock, Message, TokenUsage, StreamEvent, Result, AssistantMessage
import loom.query.wire_protocol;    // WireBackend (for BackendFactory)
import loom.hooks.tool_permissions; // PermissionContext, PermissionResponse
// ToolRegistry is needed for the register_extra_tools field type; the
// detector does not harvest the class name past loom.tools.tool's
// concept/requires blocks (same marker as query_assembly.cppm).
import loom.tools.tool;  // arch-check: keep-import

export namespace loom::sdk {

/// Per-turn options. Only fields with a per-turn engine seam are exposed;
/// construction-time config lives in HarnessConfig (see the field mapping
/// below and §2.2).
struct TurnOptions {
    std::string prompt;
    /// Via QueryEngine::set_model_params (per-turn runtime setter).
    std::optional<std::string> model;
    /// Subset of tools to enable this turn -> QueryOptions::enabled_tools.
    std::optional<std::vector<std::string>> allowed_tools;
    /// @-mention file attachments -> QueryOptions::attachments.
    std::vector<loom::core::ContentBlock> attachments;
};

/// One turn's result (maps loom::core::QueryResponse).
struct TurnResult {
    loom::core::AssistantMessage message;
    loom::core::TokenUsage usage;
    std::uint32_t tool_rounds = 0;
    std::chrono::milliseconds elapsed{0};
    bool budget_exceeded = false;
    bool success = true;
    std::vector<std::string> errors;
};

/// Ask-user permission callback (bridges ToolPermissionHook). Invoked when
/// a tool requires permission and no session rule/auto-approve applies.
using PermissionCallback =
    std::function<loom::hooks::PermissionResponse(const loom::hooks::PermissionContext&)>;

/// Streaming event sink (bridges StreamCallback).
using EventSink = std::function<void(const loom::core::StreamEvent&)>;

/// Wire backend factory. When unset, the engine builds its default
/// Messages API/OpenAI backend from QueryEngineConfig.
///
/// SCOPE (§2.4): this seam intercepts request-body serialization only —
/// make_wire_backend() is called solely from build_request_body and only
/// prepare() is used. The transport (httplib POST, SSE streaming, response
/// parsing) does NOT go through WireBackend, so a factory returning a mock
/// backend does not prevent real HTTP calls. For a no-network test, use a
/// loopback HTTP server, not a mock backend.
using BackendFactory =
    std::function<std::unique_ptr<loom::query::wire::WireBackend>()>;

/// API key provider. Called at construction; the key is never stored in the
/// config struct (so it cannot be serialized by accident).
using ApiKeyProvider = std::function<std::string()>;

/// Construction configuration. Fields with no per-turn engine seam
/// (max_turns, max_budget_usd, system_prompt, always_deny_rules) are
/// construction-time only — varying them requires a new Harness (§2.2).
struct HarnessConfig {
    std::string model;
    std::filesystem::path cwd;
    std::optional<double> max_budget_usd;
    std::optional<std::uint32_t> max_turns;
    std::optional<std::string> system_prompt;
    std::optional<std::string> append_system_prompt;
    std::optional<std::string> base_url;
    std::optional<std::string> wire_api;  // "messages" | "openai"
    /// Injected, never stored. When unset, the key falls back to settings;
    /// when that is also empty and base_url is set, a placeholder key is
    /// used (a loopback/gateway ignores it).
    ApiKeyProvider api_key_provider;
    /// Optional WireBackend seam (body-serialization only — see BackendFactory).
    BackendFactory backend_factory;
    /// Optional; absent = engine default (auto-approve).
    PermissionCallback permission_callback;
    /// Optional construction-time event sink for run() (stream() takes a
    /// per-call sink).
    EventSink event_sink;
    std::optional<std::filesystem::path> sessions_dir;  // resume/fork
    std::optional<std::filesystem::path> dump_prompts_dir;
    std::vector<std::string> always_deny_rules;
    /// Test seam: register extra tools (e.g. a mock permission-gated tool)
    /// into the assembly's ToolRegistry before the config.tools snapshot
    /// (maps to AssemblyConfig::register_extra_tools, §2.1/§4.4).
    std::function<void(loom::core::ToolRegistry&)> register_extra_tools;
};

/// Opaque embedding handle. Move-only; non-copyable.
class Harness {
public:
    explicit Harness(HarnessConfig config);
    ~Harness();
    Harness(Harness&&) noexcept;
    Harness& operator=(Harness&&) noexcept;
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    /// Run one turn to completion (blocking). Wraps QueryEngine::query.
    /// A pre-run abort() makes this return an error once ("Query
    /// interrupted"), then the flag clears so the next turn proceeds (§2.3).
    [[nodiscard]] loom::core::Result<TurnResult> run(const TurnOptions& options);

    /// Run one turn with streaming events. Wraps QueryEngine::stream_query.
    /// Errors (construction failure, pre-run abort) are delivered to `sink`
    /// as StreamError events (the engine's stream_query is void).
    void stream(const TurnOptions& options, const EventSink& sink);

    /// Abort the in-flight turn (thread-safe). Sets the harness abort flag
    /// (wired as the engine's external abort callback) and calls
    /// QueryEngine::abort. See §2.3 for the entry/reset lifecycle.
    void abort() noexcept;

    /// Resume a prior session from disk (requires sessions_dir). Uses the
    /// real engine resume path: loom.session::load_messages ->
    /// parse_session_message_value -> QueryEngine::restore_conversation
    /// (§2.3). No loom.server import.
    [[nodiscard]] loom::core::Result<void> resume(std::string_view session_id);

    /// Current session id (for resume/fork correlation).
    [[nodiscard]] std::string session_id() const;

    /// Current conversation (thread-safe copy).
    [[nodiscard]] std::vector<loom::core::Message> conversation() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace loom::sdk
