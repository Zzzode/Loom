// User-setting metadata lookup for loom.config.config.
module;

module loom.config.config;

import std;
import loom.types.tool_types;
import loom.types.types;
import loom.serdes.json;

namespace loom::core {

[[nodiscard]] const UserSettingSpec*
ConfigManager::find_user_setting(std::string_view dotted) noexcept {
    for (const auto& spec : user_setting_specs()) {
        if (spec.key == dotted) return &spec;
    }
    return nullptr;
}


VoidResult ConfigManager::merge_model_context_limits(loom::utils::json::JsonVal root) {
    const auto values = root.get("modelContextLimits");
    if (!values.valid()) return {};
    const auto invalid = [](std::size_t index, std::string_view reason) {
        return Error::make(ErrorCode::ConfigParseError,
            std::format("Invalid modelContextLimits[{}]: {}", index, reason));
    };
    if (!values.is_arr()) return std::unexpected(invalid(0, "must be an array"));
    using Identity = std::tuple<std::string, std::string, std::string>;
    const auto identity = [](const ModelContextLimit& value) -> Identity {
        return {value.model, value.base_url.value_or(""), value.wire_api.value_or("")};
    };
    std::set<Identity> seen;
    std::vector<ModelContextLimit> incoming;
    std::optional<Error> error;
    std::size_t index = 0;
    values.iter([&](loom::utils::json::JsonVal value) {
        const auto current = index++;
        if (error) return;
        const auto fail = [&](std::string_view reason) { error = invalid(current, reason); };
        if (!value.is_obj()) { fail("entry must be an object"); return; }
        const auto model = value.get("model");
        if (!model.is_str() || trim_setting_text(model.as_str()).empty() ||
            std::ranges::any_of(model.as_str(), [](unsigned char c) { return c < 0x20 || c == 0x7f; })) {
            fail("model must be a nonempty identifier"); return;
        }
        const auto window = integral_json_number(value.get("contextWindowSize"), "contextWindowSize");
        if (!window || *window <= 0 || static_cast<std::uint64_t>(*window) > std::numeric_limits<std::uint32_t>::max()) {
            fail("contextWindowSize must be a positive uint32 integer"); return;
        }
        ModelContextLimit parsed;
        parsed.model = std::string(model.as_str());
        parsed.context_window_tokens = static_cast<std::uint32_t>(*window);
        if (const auto cap = value.get("maxOutputTokens"); cap.valid()) {
            const auto output = integral_json_number(cap, "maxOutputTokens");
            if (!output || *output <= 0 || static_cast<std::uint64_t>(*output) > std::numeric_limits<std::uint32_t>::max()) {
                fail("maxOutputTokens must be a positive uint32 integer"); return;
            }
            parsed.max_output_tokens = static_cast<std::uint32_t>(*output);
        }
        const auto endpoint = value.get("baseUrl");
        const auto wire = value.get("wireApi");
        if (endpoint.valid() != wire.valid()) { fail("baseUrl and wireApi must appear together"); return; }
        if (endpoint.valid()) {
            if (!endpoint.is_str() || !wire.is_str() ||
                (wire.as_str() != "messages" && wire.as_str() != "openai")) {
                fail("baseUrl must be a URL and wireApi must be messages or openai"); return;
            }
            const auto url = endpoint.as_str();
            const auto scheme = url.starts_with("https://") ? 8u : url.starts_with("http://") ? 7u : 0u;
            const auto authority_end = url.find_first_of("/?#", scheme);
            const auto authority = url.substr(scheme, authority_end == std::string_view::npos
                ? std::string_view::npos : authority_end - scheme);
            if (scheme == 0 || authority.empty() || authority.find('@') != std::string_view::npos ||
                url.find_first_of("?#") != std::string_view::npos ||
                std::ranges::any_of(url, [](unsigned char c) { return c <= 0x20 || c == 0x7f; })) {
                fail("baseUrl must be an HTTP(S) endpoint without credentials, query or fragment"); return;
            }
            parsed.base_url = std::string(url);
            while (parsed.base_url->ends_with('/')) parsed.base_url->pop_back();
            parsed.wire_api = std::string(wire.as_str());
        }
        if (!seen.insert(identity(parsed)).second) { fail("duplicate model/endpoint/wire tuple"); return; }
        incoming.push_back(std::move(parsed));
    });
    if (error) return std::unexpected(std::move(*error));
    if (incoming.empty()) { settings_.model.context_limits.clear(); return {}; }
    for (auto& value : incoming) {
        const auto found = std::ranges::find_if(settings_.model.context_limits,
            [&](const auto& existing) { return identity(existing) == identity(value); });
        if (found == settings_.model.context_limits.end()) settings_.model.context_limits.push_back(std::move(value));
        else *found = std::move(value);
    }
    return {};
}

} // namespace loom::core
