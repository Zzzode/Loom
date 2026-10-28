/// RFC 0026: exact, endpoint-scoped model context and output resolution.
export module loom.services.compact.model_context;

import std;
import loom.types.tool_types;

export namespace loom::services::compact {

/// Parse endpoint-scoped capacity declarations without performing network I/O.
[[nodiscard]] std::vector<loom::core::ModelContextLimit>
parse_model_metadata(std::string_view body, std::string_view base_url,
                     std::string_view canonical_wire);

[[nodiscard]] loom::core::ResolvedModelContext
resolve_model_context(const loom::core::ModelContextConfig& config, std::string_view model,
                      std::string_view base_url, std::string_view wire_api,
                      std::uint32_t requested_output, std::uint64_t minimum_auto_output = 0);

}  // namespace loom::services::compact
