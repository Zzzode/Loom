/// RFC 0025: provider-neutral budgeting and semantic checkpoint policy.
export module loom.services.compact.context_compaction;

import std;
import loom.types.types;

export namespace loom::services::compact {

struct ContextPolicy {
    std::uint64_t window_tokens = 200000;
    std::uint64_t output_tokens = 16384;
    double threshold = 0.9;
};

struct UsageObservation {
    std::uint64_t input_tokens = 0;
    std::uint64_t estimated_tokens = 0;
};

struct RequestContext {
    std::span<const loom::core::Message> messages;
    std::string_view system_prompt;
    std::string_view tool_schema;
    std::optional<std::size_t> request_anchor;
    UsageObservation observed;
};

struct SummaryRequest {
    std::string instructions;
    std::string transcript;
    std::uint32_t max_output_tokens = 0;
};

struct SummaryResponse {
    std::string text;
    bool complete = false;
    bool has_tool_use = false;
    loom::core::TokenUsage usage;
};

using SummaryCallback = std::function<loom::core::Result<SummaryResponse>(const SummaryRequest&)>;
using CancellationCheck = std::function<bool()>;

struct CompactionResult {
    std::vector<std::size_t> retained_indices;
    std::string summary;
    std::uint64_t before_tokens = 0;
    std::uint64_t after_tokens = 0;
    std::uint32_t summary_requests = 0;
};

enum class ContextFailure { None, ContextTokensExceeded, RequestBytesExceeded };

[[nodiscard]] std::uint64_t estimate_message_tokens(const loom::core::Message& message);
[[nodiscard]] std::uint64_t estimate_request_tokens(const RequestContext& context,
                                                    bool calibrate = true);
[[nodiscard]] std::uint64_t usable_input_tokens(const ContextPolicy& policy);
[[nodiscard]] ContextFailure classify_context_failure(int status, std::string_view body);
[[nodiscard]] std::string checkpoint_instructions();
[[nodiscard]] bool valid_checkpoint(std::string_view text);
[[nodiscard]] bool valid_tool_history(std::span<const loom::core::Message> messages);
[[nodiscard]] loom::core::Result<CompactionResult>
compact_context(const RequestContext& context, const ContextPolicy& policy,
                const SummaryCallback& summarize, const CancellationCheck& cancelled = {});

}  // namespace loom::services::compact
