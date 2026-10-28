module loom.services.compact.model_context;

import std;
import loom.types.tool_types;
import loom.serdes.json;

namespace loom::services::compact {
namespace {
std::string_view endpoint_identity(std::string_view endpoint) {
    while (!endpoint.empty() && endpoint.back() == '/')
        endpoint.remove_suffix(1);
    return endpoint;
}

std::optional<std::pair<std::uint32_t, std::uint32_t>>
catalog_limits(std::string_view model, std::string_view endpoint, std::string_view wire) {
    // Verified 2026-10-19. Exact IDs only; custom gateways never inherit these.
    // https://developers.openai.com/api/docs/models/gpt-4.1
    if (wire == "openai" &&
        (endpoint == "https://api.openai.com" || endpoint == "https://api.openai.com/v1")) {
        if (model == "gpt-4.1" || model == "gpt-4.1-2025-04-14")
            return std::pair<std::uint32_t, std::uint32_t>{1047576, 32768};
    }
    // https://platform.claude.com/docs/en/models/haiku-4-5/overview
    // https://platform.claude.com/docs/zh-CN/models/sonnet-4-5/overview
    // https://platform.claude.com/docs/id/models/opus-4-5/overview
    // The legacy Sonnet 4.5 1M beta was retired 2026-04-30. No beta inference.
    if (wire == "messages" &&
        (endpoint == "https://api.anthropic.com" || endpoint == "https://api.anthropic.com/v1")) {
        constexpr std::array<std::string_view, 3> models{
            "claude-haiku-4-5-20251001", "claude-sonnet-4-5-20250929", "claude-opus-4-5-20251101"};
        if (std::ranges::contains(models, model))
            return std::pair<std::uint32_t, std::uint32_t>{200000, 64000};
    }
    return {};
}
}  // namespace

std::vector<loom::core::ModelContextLimit> parse_model_metadata(std::string_view body,
                                                                std::string_view base_url,
                                                                std::string_view canonical_wire) {
    auto parsed = loom::utils::json::parse(body);
    if (!parsed || !parsed->root().is_obj() || !parsed->root().get("data").is_arr()) return {};
    const auto positive_uint32 =
        [](loom::utils::json::JsonVal value) -> std::optional<std::uint32_t> {
        if (!value.is_int()) return {};
        const auto number = value.as_int();
        if (number <= 0 ||
            static_cast<std::uint64_t>(number) > std::numeric_limits<std::uint32_t>::max())
            return {};
        return static_cast<std::uint32_t>(number);
    };
    std::vector<loom::core::ModelContextLimit> limits;
    std::unordered_map<std::string, std::size_t> seen;
    std::unordered_set<std::string> conflicting;
    parsed->root().get("data").iter([&](loom::utils::json::JsonVal entry) {
        if (!entry.is_obj() || !entry.get("id").is_str()) return;
        const auto id = entry.get("id").as_str();
        const auto window = positive_uint32(entry.get("context_length"));
        if (id.empty() || !window) return;
        const auto cap = positive_uint32(entry.get("max_output_tokens"));
        const auto [found, inserted] = seen.emplace(std::string(id), limits.size());
        if (!inserted) {
            const auto& prior = limits[found->second];
            if (prior.context_window_tokens != *window || prior.max_output_tokens != cap)
                conflicting.emplace(id);
            return;
        }
        limits.push_back({.model = std::string(id),
                          .context_window_tokens = *window,
                          .max_output_tokens = cap,
                          .base_url = std::string(base_url),
                          .wire_api = std::string(canonical_wire)});
    });
    std::erase_if(limits, [&](const auto& limit) { return conflicting.contains(limit.model); });
    return limits;
}

loom::core::ResolvedModelContext
resolve_model_context(const loom::core::ModelContextConfig& config, std::string_view model,
                      std::string_view base_url, std::string_view wire_api,
                      std::uint32_t requested_output, std::uint64_t minimum_auto_output) {
    using loom::core::ModelContextLimit;
    const auto endpoint = endpoint_identity(base_url);
    const ModelContextLimit* scoped = nullptr;
    const ModelContextLimit* generic = nullptr;
    for (const auto& profile : config.limits) {
        if (profile.model != model) continue;
        if (!profile.base_url && !profile.wire_api)
            generic = &profile;
        else if (profile.base_url && profile.wire_api &&
                 endpoint_identity(*profile.base_url) == endpoint && *profile.wire_api == wire_api)
            scoped = &profile;
    }
    const ModelContextLimit* metadata = nullptr;
    for (const auto& entry : config.metadata_limits) {
        if (entry.model == model && entry.base_url && entry.wire_api &&
            endpoint_identity(*entry.base_url) == endpoint && *entry.wire_api == wire_api)
            metadata = &entry;
    }
    const auto catalog = catalog_limits(model, endpoint, wire_api);
    loom::core::ResolvedModelContext result;
    result.window_tokens = config.fallback_window_tokens;
    result.output_tokens = requested_output;
    if (scoped || generic) {
        result.window_tokens = (scoped ? scoped : generic)->context_window_tokens;
        result.source = "profile";
    } else if (metadata) {
        result.window_tokens = metadata->context_window_tokens;
        result.source = "metadata";
    } else if (catalog) {
        result.window_tokens = catalog->first;
        result.source = "catalog";
    }
    if (scoped && scoped->max_output_tokens)
        result.output_cap = scoped->max_output_tokens;
    else if (generic && generic->max_output_tokens)
        result.output_cap = generic->max_output_tokens;
    if (result.output_cap)
        result.output_cap_source = "profile";
    else if (metadata && metadata->max_output_tokens) {
        result.output_cap = metadata->max_output_tokens;
        result.output_cap_source = "metadata";
    } else if (catalog) {
        result.output_cap = catalog->second;
        result.output_cap_source = "catalog";
    }
    if (requested_output != 0) {
        if (result.output_cap)
            result.output_tokens = std::min(requested_output, *result.output_cap);
        result.request_output_tokens = result.output_tokens;
    } else {
        // Reserve input space even for tiny model windows. Unknown services use
        // an estimate; OpenAI-compatible endpoints choose their own output limit.
        const auto window = static_cast<std::uint64_t>(result.window_tokens);
        const auto safety = std::max<std::uint64_t>(64, window / 50);
        const auto ceiling = window > safety ? (window - safety) / 2 : 0;
        auto target =
            std::max<std::uint64_t>(result.output_cap.value_or(16384), minimum_auto_output);
        if (result.output_cap) target = std::min<std::uint64_t>(target, *result.output_cap);
        result.output_tokens =
            static_cast<std::uint32_t>(std::max<std::uint64_t>(1, std::min(target, ceiling)));
        if (wire_api != "openai") result.request_output_tokens = result.output_tokens;
    }
    return result;
}

}  // namespace loom::services::compact
