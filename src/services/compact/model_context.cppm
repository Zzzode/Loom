/// RFC 0026: exact, endpoint-scoped model context and output resolution.
export module loom.services.compact.model_context;

import std;
import loom.types.tool_types;

export namespace loom::services::compact {

[[nodiscard]] loom::core::ResolvedModelContext resolve_model_context(
    const loom::core::ModelContextConfig& config, std::string_view model,
    std::string_view base_url, std::string_view wire_api,
    std::uint32_t requested_output);

} // namespace loom::services::compact
