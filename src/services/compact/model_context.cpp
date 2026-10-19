module loom.services.compact.model_context;

import std;
import loom.types.tool_types;

namespace loom::services::compact {
namespace {
std::string_view endpoint_identity(std::string_view endpoint) {
    while (!endpoint.empty() && endpoint.back() == '/') endpoint.remove_suffix(1);
    return endpoint;
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> catalog_limits(
    std::string_view model, std::string_view endpoint, std::string_view wire) {
    // Verified 2026-10-19. Exact IDs only; custom gateways never inherit these.
    // https://developers.openai.com/api/docs/models/gpt-4.1
    if (wire == "openai" && (endpoint == "https://api.openai.com" ||
                             endpoint == "https://api.openai.com/v1")) {
        if (model == "gpt-4.1" || model == "gpt-4.1-2025-04-14")
            return std::pair<std::uint32_t, std::uint32_t>{1047576, 32768};
    }
    // https://platform.claude.com/docs/en/models/haiku-4-5/overview
    // https://platform.claude.com/docs/zh-CN/models/sonnet-4-5/overview
    // https://platform.claude.com/docs/id/models/opus-4-5/overview
    // The legacy Sonnet 4.5 1M beta was retired 2026-04-30. No beta inference.
    if (wire == "messages" && (endpoint == "https://api.anthropic.com" ||
                               endpoint == "https://api.anthropic.com/v1")) {
        constexpr std::array<std::string_view, 3> models{
            "claude-haiku-4-5-20251001", "claude-sonnet-4-5-20250929",
            "claude-opus-4-5-20251101"};
        if (std::ranges::contains(models, model))
            return std::pair<std::uint32_t, std::uint32_t>{200000, 64000};
    }
    return {};
}
} // namespace

loom::core::ResolvedModelContext resolve_model_context(
    const loom::core::ModelContextConfig& config, std::string_view model,
    std::string_view base_url, std::string_view wire_api,
    std::uint32_t requested_output) {
    using loom::core::ModelContextLimit;
    const auto endpoint = endpoint_identity(base_url);
    const ModelContextLimit* scoped = nullptr;
    const ModelContextLimit* generic = nullptr;
    for (const auto& profile : config.limits) {
        if (profile.model != model) continue;
        if (!profile.base_url && !profile.wire_api) generic = &profile;
        else if (profile.base_url && profile.wire_api &&
                 endpoint_identity(*profile.base_url) == endpoint && *profile.wire_api == wire_api)
            scoped = &profile;
    }
    const auto catalog = catalog_limits(model, endpoint, wire_api);
    loom::core::ResolvedModelContext result;
    result.window_tokens = config.fallback_window_tokens;
    result.output_tokens = requested_output;
    if (scoped || generic) {
        result.window_tokens = (scoped ? scoped : generic)->context_window_tokens;
        result.source = "profile";
    } else if (catalog) {
        result.window_tokens = catalog->first;
        result.source = "catalog";
    }
    if (scoped && scoped->max_output_tokens) result.output_cap = scoped->max_output_tokens;
    else if (generic && generic->max_output_tokens) result.output_cap = generic->max_output_tokens;
    if (result.output_cap) result.output_cap_source = "profile";
    else if (catalog) {
        result.output_cap = catalog->second;
        result.output_cap_source = "catalog";
    }
    if (result.output_cap) result.output_tokens = std::min(requested_output, *result.output_cap);
    return result;
}

} // namespace loom::services::compact
