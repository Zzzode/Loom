/// @file tool_types.cppm
/// @brief Tool input/output DTOs shared across layers.
/// Rank-1 leaf: ToolInput/ToolOutputContent/ToolResult with zero loom.*
/// imports; loom.tools.tool re-exports it so existing importers stay
/// unchanged. has_field stays in loom.tools.tool (it needs loom.serdes.json).
export module loom.types.tool_types;

import std;

export namespace loom::core {

// ============================================================
// Tool Input / Output types
// ============================================================

/// Exact model capacities supplied by file configuration (RFC 0026).
struct ModelContextLimit {
    std::string model;
    std::uint32_t context_window_tokens = 0;
    std::optional<std::uint32_t> max_output_tokens;
    std::optional<std::string> base_url;
    std::optional<std::string> wire_api;
};

/// Original fallback and all profiles, shared immutably by execution owners.
struct ModelContextConfig {
    std::uint32_t fallback_window_tokens = 200000;
    std::vector<ModelContextLimit> limits;
};

struct ResolvedModelContext {
    std::uint32_t window_tokens = 200000;
    std::uint32_t output_tokens = 16384;
    std::optional<std::uint32_t> output_cap;
    std::string source = "fallback";
    std::string output_cap_source = "none";
};

/// Immutable authenticated session snapshot. Internal only: never write it to
/// tool JSON, transcripts, agent records, or session persistence.
struct ApiExecutionContext {
    std::string wire_api = "messages";
    std::string base_url;
    std::string api_key;
    std::string auth_token;
    std::string api_version = "2023-06-01";
    std::chrono::milliseconds timeout{120000};
    std::uint64_t context_window_tokens = 200000;
    std::uint64_t context_output_tokens = 16384;
    double context_compaction_threshold = 0.9;
    bool context_auto_compact = true;
    std::shared_ptr<const ModelContextConfig> model_context_config{};
};

/// Host-only immutable target checked atomically during resume admission.
/// Never encoded in model JSON or persisted runtime records.
struct HostAgentResumeConstraint {
    std::string agent_id;
    std::string agent_type;
    std::string parent_session_id;
    std::optional<std::string> cwd{};
    std::optional<std::string> team_name{};
    std::optional<std::string> name{};
    std::optional<std::string> teammate_backend{};
    std::optional<std::string> teammate_task_id{};
    std::optional<std::string> teammate_pane_id{};
};

/// JSON-based tool input wrapping raw parameter data
struct ToolInput {
    std::string raw_json;   // Raw JSON string of parameters
    // Internal execution context; never serialized as model-supplied input.
    std::optional<std::string> parent_model = std::nullopt;
    // Queried only during execute(); must be non-blocking and non-throwing.
    // Captures must outlive that execution.
    std::function<bool()> is_cancelled = {};
    std::shared_ptr<const ApiExecutionContext> api_context = {};
    // Root session ownership for native subagent instances; never model input.
    std::optional<std::string> parent_session_id = std::nullopt;
    std::optional<HostAgentResumeConstraint> host_agent_resume = std::nullopt;

    /// Get the raw JSON as string view
    [[nodiscard]] std::string_view json() const noexcept { return raw_json; }

    /// Create from raw JSON string
    [[nodiscard]] static ToolInput from_json(std::string json) {
        return ToolInput{std::move(json)};
    }
};

/// Content returned by tool execution, modeled after API content blocks
struct ToolOutputContent {
    std::string text;                   // Primary output text
    std::optional<std::string> format;  // "text", "json", "markdown"
    std::optional<std::string> media_type;
    std::optional<std::string> data;

    /// Create a plain text output
    [[nodiscard]] static ToolOutputContent text_output(std::string text) {
        return ToolOutputContent{
            .text = std::move(text),
            .format = "text",
            .media_type = std::nullopt,
            .data = std::nullopt,
        };
    }

    /// Create a JSON-formatted output
    [[nodiscard]] static ToolOutputContent json_output(std::string json) {
        return ToolOutputContent{
            .text = std::move(json),
            .format = "json",
            .media_type = std::nullopt,
            .data = std::nullopt,
        };
    }

    /// Create a base64 image output
    [[nodiscard]] static ToolOutputContent image_output(std::string media_type, std::string data) {
        return ToolOutputContent{
            .text = {},
            .format = "image",
            .media_type = std::move(media_type),
            .data = std::move(data),
        };
    }

    /// Create a base64 document output
    [[nodiscard]] static ToolOutputContent document_output(std::string media_type, std::string data) {
        return ToolOutputContent{
            .text = {},
            .format = "document",
            .media_type = std::move(media_type),
            .data = std::move(data),
        };
    }
};

/// Result of a tool execution
struct ToolResult {
    std::vector<ToolOutputContent> content;  // Output content blocks
    bool is_error = false;                   // Whether execution failed

    /// Create a successful single-text result
    [[nodiscard]] static ToolResult success(std::string text) {
        return ToolResult{
            {ToolOutputContent::text_output(std::move(text))},
            false
        };
    }

    /// Create an error result
    [[nodiscard]] static ToolResult error(std::string message) {
        return ToolResult{
            {ToolOutputContent::text_output(std::move(message))},
            true
        };
    }

    /// Create a multi-content successful result
    [[nodiscard]] static ToolResult success_multi(std::vector<ToolOutputContent> content) {
        return ToolResult{std::move(content), false};
    }
};

} // namespace loom::core
