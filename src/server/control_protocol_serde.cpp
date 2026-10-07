// control_protocol_serde.cpp — ser/de function definitions
// Extracted from control_protocol.cppm to reduce inline body count.
// RFC 0001 Phase C: inline def ratchet (173 -> <100).
module;

#include <cstdint>

module loom.server.control_protocol;

import std;

import loom.serdes.json;

// All ser/de function definitions (moved from the interface)


namespace loom::server::control {

[[nodiscard]] std::string SlashCommand_to_json(const SlashCommand& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("name", doc.string(v.name));
    o.add("description", doc.string(v.description));
    o.add("argument_hint", doc.string(v.argument_hint));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SlashCommand, std::string> SlashCommand_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    SlashCommand v;
    v.name = detail_serde::read_string(o, "name");
    v.description = detail_serde::read_string(o, "description");
    v.argument_hint = detail_serde::read_string(o, "argument_hint");
    return v;
}

[[nodiscard]] std::string AgentInfo_to_json(const AgentInfo& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("name", doc.string(v.name));
    o.add("description", doc.string(v.description));
    if (v.model.has_value()) o.add("model", doc.string(*v.model));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<AgentInfo, std::string> AgentInfo_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    AgentInfo v;
    v.name = detail_serde::read_string(o, "name");
    v.description = detail_serde::read_string(o, "description");
    v.model = detail_serde::read_optional_string(o, "model");
    return v;
}

[[nodiscard]] std::string ModelInfo_to_json(const ModelInfo& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("value", doc.string(v.value));
    o.add("display_name", doc.string(v.display_name));
    o.add("description", doc.string(v.description));
    if (v.supports_effort.has_value()) o.add("supports_effort", doc.boolean(*v.supports_effort));
    if (v.supported_effort_levels.has_value()) {
        auto arr = doc.array();
        for (const auto& s : *v.supported_effort_levels) arr.append(doc.string(s));
        o.add("supported_effort_levels", std::move(arr));
    }
    if (v.supports_adaptive_thinking.has_value())
        o.add("supports_adaptive_thinking", doc.boolean(*v.supports_adaptive_thinking));
    if (v.supports_fast_mode.has_value())
        o.add("supports_fast_mode", doc.boolean(*v.supports_fast_mode));
    if (v.supports_auto_mode.has_value())
        o.add("supports_auto_mode", doc.boolean(*v.supports_auto_mode));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ModelInfo, std::string> ModelInfo_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    ModelInfo v;
    v.value = detail_serde::read_string(o, "value");
    v.display_name = detail_serde::read_string(o, "display_name");
    v.description = detail_serde::read_string(o, "description");
    v.supports_effort = detail_serde::read_optional_bool(o, "supports_effort");
    v.supported_effort_levels = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("supported_effort_levels");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](JsonVal el) { if (el.is_str()) out.emplace_back(el.as_str()); });
        return out;
    }();
    v.supports_adaptive_thinking = detail_serde::read_optional_bool(o, "supports_adaptive_thinking");
    v.supports_fast_mode = detail_serde::read_optional_bool(o, "supports_fast_mode");
    v.supports_auto_mode = detail_serde::read_optional_bool(o, "supports_auto_mode");
    return v;
}

[[nodiscard]] std::string AccountInfo_to_json(const AccountInfo& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    if (v.email.has_value()) o.add("email", doc.string(*v.email));
    if (v.organization.has_value()) o.add("organization", doc.string(*v.organization));
    if (v.subscription_type.has_value()) o.add("subscription_type", doc.string(*v.subscription_type));
    if (v.token_source.has_value()) o.add("token_source", doc.string(*v.token_source));
    if (v.api_key_source.has_value()) o.add("api_key_source", doc.string(*v.api_key_source));
    if (v.api_provider.has_value()) o.add("api_provider", doc.string(*v.api_provider));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<AccountInfo, std::string> AccountInfo_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    AccountInfo v;
    v.email = detail_serde::read_optional_string(o, "email");
    v.organization = detail_serde::read_optional_string(o, "organization");
    v.subscription_type = detail_serde::read_optional_string(o, "subscription_type");
    v.token_source = detail_serde::read_optional_string(o, "token_source");
    v.api_key_source = detail_serde::read_optional_string(o, "api_key_source");
    v.api_provider = detail_serde::read_optional_string(o, "api_provider");
    return v;
}

[[nodiscard]] std::string ModelUsage_to_json(const ModelUsage& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("inputTokens", doc.number(static_cast<int64_t>(v.input_tokens)));
    o.add("outputTokens", doc.number(static_cast<int64_t>(v.output_tokens)));
    o.add("cacheReadInputTokens", doc.number(static_cast<int64_t>(v.cache_read_input_tokens)));
    o.add("cacheCreationInputTokens", doc.number(static_cast<int64_t>(v.cache_creation_input_tokens)));
    o.add("webSearchRequests", doc.number(static_cast<int64_t>(v.web_search_requests)));
    o.add("costUSD", doc.number(v.cost_usd));
    o.add("contextWindow", doc.number(static_cast<int64_t>(v.context_window)));
    o.add("maxOutputTokens", doc.number(static_cast<int64_t>(v.max_output_tokens)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ModelUsage, std::string> ModelUsage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto o = parsed->root();
    if (!o.is_obj()) return std::unexpected("expected object");
    ModelUsage v;
    v.input_tokens = static_cast<int>(o.get("inputTokens").as_int());
    v.output_tokens = static_cast<int>(o.get("outputTokens").as_int());
    v.cache_read_input_tokens = static_cast<int>(o.get("cacheReadInputTokens").as_int());
    v.cache_creation_input_tokens = static_cast<int>(o.get("cacheCreationInputTokens").as_int());
    v.web_search_requests = static_cast<int>(o.get("webSearchRequests").as_int());
    v.cost_usd = o.get("costUSD").as_double();
    v.context_window = static_cast<int>(o.get("contextWindow").as_int());
    v.max_output_tokens = static_cast<int>(o.get("maxOutputTokens").as_int());
    return v;
}

[[nodiscard]] std::string ControlInitializeRequest_to_json(const ControlInitializeRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("initialize"));
    if (v.hooks.has_value()) {
        auto hooks = doc.object();
        for (const auto& [k, matchers] : *v.hooks) {
            auto arr = doc.array();
            for (const auto& m : matchers) {
                auto mo = doc.object();
                if (m.matcher.has_value()) mo.add("matcher", doc.string(*m.matcher));
                auto ids = doc.array();
                for (const auto& id : m.hook_callback_ids) ids.append(doc.string(id));
                mo.add("hook_callback_ids", std::move(ids));
                if (m.timeout.has_value()) mo.add("timeout", doc.number(*m.timeout));
                arr.append(std::move(mo));
            }
            hooks.add(k.c_str(), std::move(arr));
        }
        o.add("hooks", std::move(hooks));
    }
    if (v.sdk_mcp_servers.has_value()) {
        auto arr = doc.array();
        for (const auto& s : *v.sdk_mcp_servers) arr.append(doc.string(s));
        o.add("sdk_mcp_servers", std::move(arr));
    }
    if (v.system_prompt.has_value()) o.add("system_prompt", doc.string(*v.system_prompt));
    if (v.append_system_prompt.has_value())
        o.add("append_system_prompt", doc.string(*v.append_system_prompt));
    if (v.prompt_suggestions.has_value())
        o.add("prompt_suggestions", doc.boolean(*v.prompt_suggestions));
    if (v.agent_progress_summaries.has_value())
        o.add("agent_progress_summaries", doc.boolean(*v.agent_progress_summaries));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::string ControlInterruptRequest_to_json(const ControlInterruptRequest&) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("interrupt"));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::string ControlPermissionRequest_to_json(const ControlPermissionRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("can_use_tool"));
    o.add("tool_name", doc.string(v.tool_name));
    // input is the canonical (parsed + re-serialized) JSON; embed verbatim.
    o.add("input", doc.raw_json(v.input_json.empty() ? "{}" : v.input_json));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    if (v.description.has_value()) o.add("description", doc.string(*v.description));
    if (v.blocked_path.has_value()) o.add("blocked_path", doc.string(*v.blocked_path));
    if (v.decision_reason.has_value()) o.add("decision_reason", doc.string(*v.decision_reason));
    if (v.title.has_value()) o.add("title", doc.string(*v.title));
    if (v.display_name.has_value()) o.add("display_name", doc.string(*v.display_name));
    if (v.agent_id.has_value()) o.add("agent_id", doc.string(*v.agent_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlPermissionRequest, std::string>
ControlPermissionRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlPermissionRequest v;
    v.tool_name = detail_serde::read_string(inner, "tool_name");
    auto input = inner.get("input");
    v.input_json = input.valid() ? input.to_string() : "{}";
    v.tool_use_id = detail_serde::read_string(inner, "tool_use_id");
    v.description = detail_serde::read_optional_string(inner, "description");
    v.blocked_path = detail_serde::read_optional_string(inner, "blocked_path");
    v.decision_reason = detail_serde::read_optional_string(inner, "decision_reason");
    v.title = detail_serde::read_optional_string(inner, "title");
    v.display_name = detail_serde::read_optional_string(inner, "display_name");
    v.agent_id = detail_serde::read_optional_string(inner, "agent_id");
    return v;
}

[[nodiscard]] std::string ControlSetPermissionModeRequest_to_json(
    const ControlSetPermissionModeRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_permission_mode"));
    o.add("mode", doc.string(std::string(detail_serde::permission_mode_to_str(v.mode))));
    if (v.ultraplan.has_value()) o.add("ultraplan", doc.boolean(*v.ultraplan));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlSetPermissionModeRequest, std::string>
ControlSetPermissionModeRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlSetPermissionModeRequest v;
    auto mode_str = detail_serde::read_string(inner, "mode");
    if (auto m = detail_serde::permission_mode_from_str(mode_str)) {
        v.mode = *m;
    }
    v.ultraplan = detail_serde::read_optional_bool(inner, "ultraplan");
    return v;
}

[[nodiscard]] std::string ControlSetModelRequest_to_json(const ControlSetModelRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_model"));
    if (v.model.has_value()) o.add("model", doc.string(*v.model));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlSetModelRequest, std::string>
ControlSetModelRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlSetModelRequest v;
    v.model = detail_serde::read_optional_string(inner, "model");
    return v;
}

[[nodiscard]] std::string ControlSetMaxThinkingTokensRequest_to_json(
    const ControlSetMaxThinkingTokensRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("set_max_thinking_tokens"));
    if (v.max_thinking_tokens.has_value())
        o.add("max_thinking_tokens", doc.number(static_cast<int64_t>(*v.max_thinking_tokens)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlSetMaxThinkingTokensRequest, std::string>
ControlSetMaxThinkingTokensRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlSetMaxThinkingTokensRequest v;
    if (auto t = detail_serde::read_optional_int(inner, "max_thinking_tokens")) {
        v.max_thinking_tokens = static_cast<int>(*t);
    }
    return v;
}

[[nodiscard]] std::string ControlRawRequest_to_json(const ControlRawRequest& v) {
    // The raw JSON already includes the subtype field.
    return v.raw_json.empty() ? "{}" : v.raw_json;
}

[[nodiscard]] std::string McpStdioServerConfig_to_json(const McpStdioServerConfig& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("stdio"));
    o.add("command", doc.string(v.command));
    if (v.args) {
        auto arr = doc.array();
        for (const auto& a : *v.args) arr.append(doc.string(a));
        o.add("args", std::move(arr));
    }
    if (v.env) {
        auto e = doc.object();
        for (const auto& [k, val] : *v.env) e.add(k.c_str(), doc.string(val));
        o.add("env", std::move(e));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<McpStdioServerConfig, std::string>
McpStdioServerConfig_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    McpStdioServerConfig v;
    v.command = detail_serde::read_string(o, "command");
    if (auto arr = o.get("args"); arr.is_arr()) {
        std::vector<std::string> args;
        arr.iter([&](JsonVal el) { if (el.is_str()) args.emplace_back(el.as_str()); });
        v.args = std::move(args);
    }
    if (auto m = o.get("env"); m.is_obj()) {
        std::unordered_map<std::string, std::string> env;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                env[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.env = std::move(env);
    }
    return v;
}

[[nodiscard]] std::string McpSSEServerConfig_to_json(const McpSSEServerConfig& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("sse"));
    o.add("url", doc.string(v.url));
    if (v.headers) {
        auto h = doc.object();
        for (const auto& [k, val] : *v.headers) h.add(k.c_str(), doc.string(val));
        o.add("headers", std::move(h));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<McpSSEServerConfig, std::string>
McpSSEServerConfig_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    McpSSEServerConfig v;
    v.url = detail_serde::read_string(o, "url");
    if (auto m = o.get("headers"); m.is_obj()) {
        std::unordered_map<std::string, std::string> headers;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                headers[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.headers = std::move(headers);
    }
    return v;
}

[[nodiscard]] std::string McpHttpServerConfig_to_json(const McpHttpServerConfig& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("http"));
    o.add("url", doc.string(v.url));
    if (v.headers) {
        auto h = doc.object();
        for (const auto& [k, val] : *v.headers) h.add(k.c_str(), doc.string(val));
        o.add("headers", std::move(h));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<McpHttpServerConfig, std::string>
McpHttpServerConfig_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    McpHttpServerConfig v;
    v.url = detail_serde::read_string(o, "url");
    if (auto m = o.get("headers"); m.is_obj()) {
        std::unordered_map<std::string, std::string> headers;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                headers[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.headers = std::move(headers);
    }
    return v;
}

[[nodiscard]] std::string McpSdkServerConfig_to_json(const McpSdkServerConfig& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("sdk"));
    o.add("name", doc.string(v.name));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<McpSdkServerConfig, std::string>
McpSdkServerConfig_from_json(loom::utils::json::JsonVal o) {
    McpSdkServerConfig v;
    v.name = detail_serde::read_string(o, "name");
    return v;
}

[[nodiscard]] std::string McpServerConfig_to_json(const McpServerConfig& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, McpStdioServerConfig>)
                return McpStdioServerConfig_to_json(alt);
            else if constexpr (std::is_same_v<T, McpSSEServerConfig>)
                return McpSSEServerConfig_to_json(alt);
            else if constexpr (std::is_same_v<T, McpHttpServerConfig>)
                return McpHttpServerConfig_to_json(alt);
            else
                return McpSdkServerConfig_to_json(alt);
        },
        v);
}

[[nodiscard]] std::expected<McpServerConfig, std::string>
McpServerConfig_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    if (!o.is_obj()) return std::unexpected("McpServerConfig: expected object");
    const auto type = detail_serde::read_string(o, "type");
    if (type == "stdio") {
        auto r = McpStdioServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "sse") {
        auto r = McpSSEServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "http") {
        auto r = McpHttpServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    if (type == "sdk") {
        auto r = McpSdkServerConfig_from_json(o);
        if (!r) return std::unexpected(r.error());
        return McpServerConfig{std::move(*r)};
    }
    return std::unexpected("McpServerConfig: unknown transport type: " + type);
}

[[nodiscard]] std::expected<McpServerConfig, std::string>
McpServerConfig_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return McpServerConfig_from_json(parsed->root());
}

[[nodiscard]] std::expected<ControlInitializeRequest, std::string>
ControlInitializeRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlInitializeRequest v;
    if (auto hooks = inner.get("hooks"); hooks.is_obj()) {
        std::unordered_map<std::string, std::vector<HookCallbackMatcher>> out;
        hooks.iter_obj([&](JsonVal key, JsonVal val) {
            if (!key.is_str() || !val.is_arr()) return;
            std::vector<HookCallbackMatcher> matchers;
            val.iter([&](JsonVal el) {
                if (!el.is_obj()) return;
                HookCallbackMatcher m;
                m.matcher = detail_serde::read_optional_string(el, "matcher");
                m.hook_callback_ids = detail_serde::read_string_vec(el, "hook_callback_ids");
                if (auto t = detail_serde::read_optional_double(el, "timeout"))
                    m.timeout = *t;
                matchers.push_back(std::move(m));
            });
            out[std::string(key.as_str())] = std::move(matchers);
        });
        v.hooks = std::move(out);
    }
    if (auto arr = inner.get("sdk_mcp_servers"); arr.is_arr()) {
        std::vector<std::string> servers;
        arr.iter([&](JsonVal el) { if (el.is_str()) servers.emplace_back(el.as_str()); });
        v.sdk_mcp_servers = std::move(servers);
    }
    v.system_prompt = detail_serde::read_optional_string(inner, "system_prompt");
    v.append_system_prompt = detail_serde::read_optional_string(inner, "append_system_prompt");
    v.prompt_suggestions = detail_serde::read_optional_bool(inner, "prompt_suggestions");
    v.agent_progress_summaries =
        detail_serde::read_optional_bool(inner, "agent_progress_summaries");
    return v;
}

[[nodiscard]] std::string ControlMcpStatusRequest_to_json(
    const ControlMcpStatusRequest&
) {
    return R"({"subtype":"mcp_status"})";
}

[[nodiscard]] std::string ControlGetContextUsageRequest_to_json(
    const ControlGetContextUsageRequest&
) {
    return R"({"subtype":"get_context_usage"})";
}

[[nodiscard]] std::string ControlRewindFilesRequest_to_json(
    const ControlRewindFilesRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("rewind_files"));
    o.add("user_message_id", doc.string(v.user_message_id));
    if (v.dry_run) o.add("dry_run", doc.boolean(*v.dry_run));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlRewindFilesRequest, std::string>
ControlRewindFilesRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlRewindFilesRequest v;
    v.user_message_id = detail_serde::read_string(inner, "user_message_id");
    v.dry_run = detail_serde::read_optional_bool(inner, "dry_run");
    return v;
}

[[nodiscard]] std::string ControlCancelAsyncMessageRequest_to_json(
    const ControlCancelAsyncMessageRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("cancel_async_message"));
    o.add("message_uuid", doc.string(v.message_uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlCancelAsyncMessageRequest, std::string>
ControlCancelAsyncMessageRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlCancelAsyncMessageRequest v;
    v.message_uuid = detail_serde::read_string(inner, "message_uuid");
    return v;
}

[[nodiscard]] std::string ControlSeedReadStateRequest_to_json(
    const ControlSeedReadStateRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("seed_read_state"));
    o.add("path", doc.string(v.path));
    o.add("mtime", doc.number(v.mtime));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlSeedReadStateRequest, std::string>
ControlSeedReadStateRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlSeedReadStateRequest v;
    v.path = detail_serde::read_string(inner, "path");
    if (auto m = inner.get("mtime"); m.is_num()) v.mtime = m.as_double();
    return v;
}

[[nodiscard]] std::string HookCallbackRequest_to_json(const HookCallbackRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("hook_callback"));
    o.add("callback_id", doc.string(v.callback_id));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    auto input = doc.object();
    for (const auto& [k, val] : v.input) input.add(k.c_str(), doc.string(val));
    o.add("input", std::move(input));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<HookCallbackRequest, std::string>
HookCallbackRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    HookCallbackRequest v;
    v.callback_id = detail_serde::read_string(inner, "callback_id");
    v.tool_use_id = detail_serde::read_string(inner, "tool_use_id");
    if (auto m = inner.get("input"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                v.input[std::string(k.as_str())] = std::string(val.as_str());
        });
    }
    return v;
}

[[nodiscard]] std::string ControlMcpMessageRequest_to_json(
    const ControlMcpMessageRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_message"));
    o.add("server_name", doc.string(v.server_name));
    o.add("message", doc.string(v.message));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlMcpMessageRequest, std::string>
ControlMcpMessageRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlMcpMessageRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    v.message = detail_serde::read_string(inner, "message");
    return v;
}

[[nodiscard]] std::string ControlMcpSetServersRequest_to_json(
    const ControlMcpSetServersRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_set_servers"));
    auto servers = doc.object();
    for (const auto& [k, cfg] : v.servers)
        servers.add(k.c_str(), doc.raw_json(McpServerConfig_to_json(cfg)));
    o.add("servers", std::move(servers));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlMcpSetServersRequest, std::string>
ControlMcpSetServersRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlMcpSetServersRequest v;
    if (auto m = inner.get("servers"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto cfg = McpServerConfig_from_json(val);
            if (cfg) v.servers[std::string(k.as_str())] = std::move(*cfg);
        });
    }
    return v;
}

[[nodiscard]] std::string ControlReloadPluginsRequest_to_json(
    const ControlReloadPluginsRequest&
) {
    return R"({"subtype":"reload_plugins"})";
}

[[nodiscard]] std::string ControlMcpReconnectRequest_to_json(
    const ControlMcpReconnectRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_reconnect"));
    o.add("server_name", doc.string(v.server_name));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlMcpReconnectRequest, std::string>
ControlMcpReconnectRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlMcpReconnectRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    return v;
}

[[nodiscard]] std::string ControlMcpToggleRequest_to_json(
    const ControlMcpToggleRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("mcp_toggle"));
    o.add("server_name", doc.string(v.server_name));
    o.add("enabled", doc.boolean(v.enabled));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlMcpToggleRequest, std::string>
ControlMcpToggleRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlMcpToggleRequest v;
    v.server_name = detail_serde::read_string(inner, "server_name");
    if (auto e = inner.get("enabled"); e.is_bool()) v.enabled = e.as_bool();
    return v;
}

[[nodiscard]] std::string ControlStopTaskRequest_to_json(
    const ControlStopTaskRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("stop_task"));
    o.add("task_id", doc.string(v.task_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlStopTaskRequest, std::string>
ControlStopTaskRequest_from_json(loom::utils::json::JsonVal inner) {
    ControlStopTaskRequest v;
    v.task_id = detail_serde::read_string(inner, "task_id");
    return v;
}

[[nodiscard]] std::string ControlApplyFlagSettingsRequest_to_json(
    const ControlApplyFlagSettingsRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("apply_flag_settings"));
    auto settings = doc.object();
    for (const auto& [k, val] : v.settings) settings.add(k.c_str(), doc.string(val));
    o.add("settings", std::move(settings));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlApplyFlagSettingsRequest, std::string>
ControlApplyFlagSettingsRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlApplyFlagSettingsRequest v;
    if (auto m = inner.get("settings"); m.is_obj()) {
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                v.settings[std::string(k.as_str())] = std::string(val.as_str());
        });
    }
    return v;
}

[[nodiscard]] std::string ControlGetSettingsRequest_to_json(
    const ControlGetSettingsRequest&
) {
    return R"({"subtype":"get_settings"})";
}

[[nodiscard]] std::string ControlElicitationRequest_to_json(
    const ControlElicitationRequest& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("elicitation"));
    o.add("mcp_server_name", doc.string(v.mcp_server_name));
    o.add("message", doc.string(v.message));
    if (v.mode) o.add("mode", doc.string(*v.mode));
    if (v.url) o.add("url", doc.string(*v.url));
    if (v.elicitation_id) o.add("elicitation_id", doc.string(*v.elicitation_id));
    if (v.requested_schema) {
        auto rs = doc.object();
        for (const auto& [k, val] : *v.requested_schema) rs.add(k.c_str(), doc.string(val));
        o.add("requested_schema", std::move(rs));
    }
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlElicitationRequest, std::string>
ControlElicitationRequest_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlElicitationRequest v;
    v.mcp_server_name = detail_serde::read_string(inner, "mcp_server_name");
    v.message = detail_serde::read_string(inner, "message");
    v.mode = detail_serde::read_optional_string(inner, "mode");
    v.url = detail_serde::read_optional_string(inner, "url");
    v.elicitation_id = detail_serde::read_optional_string(inner, "elicitation_id");
    if (auto m = inner.get("requested_schema"); m.is_obj()) {
        std::unordered_map<std::string, std::string> rs;
        m.iter_obj([&](JsonVal k, JsonVal val) {
            if (k.is_str() && val.is_str())
                rs[std::string(k.as_str())] = std::string(val.as_str());
        });
        v.requested_schema = std::move(rs);
    }
    return v;
}

[[nodiscard]] std::string control_request_inner_to_json(const ControlRequestInner& inner) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlInitializeRequest>)
                return ControlInitializeRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlInterruptRequest>)
                return ControlInterruptRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlPermissionRequest>)
                return ControlPermissionRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetPermissionModeRequest>)
                return ControlSetPermissionModeRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetModelRequest>)
                return ControlSetModelRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSetMaxThinkingTokensRequest>)
                return ControlSetMaxThinkingTokensRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpStatusRequest>)
                return ControlMcpStatusRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlGetContextUsageRequest>)
                return ControlGetContextUsageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRewindFilesRequest>)
                return ControlRewindFilesRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlCancelAsyncMessageRequest>)
                return ControlCancelAsyncMessageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlSeedReadStateRequest>)
                return ControlSeedReadStateRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, HookCallbackRequest>)
                return HookCallbackRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpMessageRequest>)
                return ControlMcpMessageRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpSetServersRequest>)
                return ControlMcpSetServersRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlReloadPluginsRequest>)
                return ControlReloadPluginsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpReconnectRequest>)
                return ControlMcpReconnectRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlMcpToggleRequest>)
                return ControlMcpToggleRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlStopTaskRequest>)
                return ControlStopTaskRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlApplyFlagSettingsRequest>)
                return ControlApplyFlagSettingsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlGetSettingsRequest>)
                return ControlGetSettingsRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlElicitationRequest>)
                return ControlElicitationRequest_to_json(alt);
            else
                return ControlRawRequest_to_json(alt);
        },
        inner);
}

[[nodiscard]] std::string control_request_subtype_str(const ControlRequestInner& inner) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlRawRequest>)
                return alt.subtype;
            else
                return std::string(T::subtype);
        },
        inner);
}

[[nodiscard]] ControlRequestInner parse_control_request_inner(
    std::string_view subtype,
    loom::utils::json::JsonVal inner
) {
    if (subtype == "initialize") {
        auto r = ControlInitializeRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "interrupt") return ControlInterruptRequest{};
    if (subtype == "can_use_tool") {
        auto r = ControlPermissionRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_permission_mode") {
        auto r = ControlSetPermissionModeRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_model") {
        auto r = ControlSetModelRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "set_max_thinking_tokens") {
        auto r = ControlSetMaxThinkingTokensRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_status") return ControlMcpStatusRequest{};
    if (subtype == "get_context_usage") return ControlGetContextUsageRequest{};
    if (subtype == "rewind_files") {
        auto r = ControlRewindFilesRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "cancel_async_message") {
        auto r = ControlCancelAsyncMessageRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "seed_read_state") {
        auto r = ControlSeedReadStateRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "hook_callback") {
        auto r = HookCallbackRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_message") {
        auto r = ControlMcpMessageRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_set_servers") {
        auto r = ControlMcpSetServersRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "reload_plugins") return ControlReloadPluginsRequest{};
    if (subtype == "mcp_reconnect") {
        auto r = ControlMcpReconnectRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "mcp_toggle") {
        auto r = ControlMcpToggleRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "stop_task") {
        auto r = ControlStopTaskRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "apply_flag_settings") {
        auto r = ControlApplyFlagSettingsRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    if (subtype == "get_settings") return ControlGetSettingsRequest{};
    if (subtype == "elicitation") {
        auto r = ControlElicitationRequest_from_json(inner);
        if (r) return std::move(*r);
        return ControlRawRequest{std::string(subtype), inner.to_string()};
    }
    // Unrecognized subtype — raw passthrough.
    return ControlRawRequest{std::string(subtype), inner.to_string()};
}

[[nodiscard]] std::string ControlInitializeResponse_to_json(
    const ControlInitializeResponse& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    // commands: always emitted (even when empty)
    auto cmds = doc.array();
    for (const auto& c : v.commands) cmds.append(doc.raw_json(SlashCommand_to_json(c)));
    o.add("commands", std::move(cmds));
    // agents: emitted only when non-empty
    if (!v.agents.empty()) {
        auto agents = doc.array();
        for (const auto& a : v.agents) agents.append(doc.raw_json(AgentInfo_to_json(a)));
        o.add("agents", std::move(agents));
    }
    o.add("output_style", doc.string(v.output_style));
    auto styles = doc.array();
    for (const auto& s : v.available_output_styles) styles.append(doc.string(s));
    o.add("available_output_styles", std::move(styles));
    // models: always emitted (even when empty)
    auto models = doc.array();
    for (const auto& m : v.models) models.append(doc.raw_json(ModelInfo_to_json(m)));
    o.add("models", std::move(models));
    // account: always emitted (even when empty)
    o.add("account", doc.raw_json(AccountInfo_to_json(v.account)));
    if (v.pid.has_value())
        o.add("pid", doc.number(static_cast<int64_t>(*v.pid)));
    if (v.fast_mode_state.has_value())
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlInitializeResponse, std::string>
ControlInitializeResponse_from_json(loom::utils::json::JsonVal inner) {
    using namespace loom::utils::json;
    ControlInitializeResponse v;
    v.commands = [&] {
        std::vector<SlashCommand> out;
        auto arr = inner.get("commands");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = SlashCommand_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    v.agents = [&] {
        std::vector<AgentInfo> out;
        auto arr = inner.get("agents");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = AgentInfo_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    v.output_style = detail_serde::read_string(inner, "output_style");
    v.available_output_styles = detail_serde::read_string_vec(inner, "available_output_styles");
    v.models = [&] {
        std::vector<ModelInfo> out;
        auto arr = inner.get("models");
        if (arr.is_arr()) {
            arr.iter([&](JsonVal el) {
                if (auto r = ModelInfo_from_json(el.to_string())) out.push_back(*r);
            });
        }
        return out;
    }();
    if (auto acc = inner.get("account"); acc.is_obj()) {
        if (auto r = AccountInfo_from_json(acc.to_string())) v.account = *r;
    }
    if (auto p = detail_serde::read_optional_int(inner, "pid")) v.pid = static_cast<int>(*p);
    if (auto fms = inner.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    return v;
}

[[nodiscard]] std::expected<ControlInitializeResponse, std::string>
ControlInitializeResponse_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlInitializeResponse_from_json(parsed->root());
}

[[nodiscard]] std::string ControlSuccessResponse_to_json(const ControlSuccessResponse& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("success"));
    o.add("request_id", doc.string(v.request_id));
    // response is always present on the wire (empty object when unset).
    o.add("response", doc.raw_json(v.response_json.value_or("{}")));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlSuccessResponse, std::string>
ControlSuccessResponse_from_json(loom::utils::json::JsonVal inner) {
    ControlSuccessResponse v;
    v.request_id = detail_serde::read_string(inner, "request_id");
    auto resp = inner.get("response");
    if (resp.valid() && !resp.is_null()) {
        v.response_json = resp.to_string();
    }
    return v;
}

[[nodiscard]] std::expected<ControlSuccessResponse, std::string>
ControlSuccessResponse_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlSuccessResponse_from_json(parsed->root());
}

[[nodiscard]] std::string ControlErrorResponse_to_json(const ControlErrorResponse& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("subtype", doc.string("error"));
    o.add("request_id", doc.string(v.request_id));
    o.add("error", doc.string(v.error));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlErrorResponse, std::string>
ControlErrorResponse_from_json(loom::utils::json::JsonVal inner) {
    ControlErrorResponse v;
    v.request_id = detail_serde::read_string(inner, "request_id");
    v.error = detail_serde::read_string(inner, "error");
    return v;
}

[[nodiscard]] std::expected<ControlErrorResponse, std::string>
ControlErrorResponse_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return ControlErrorResponse_from_json(parsed->root());
}

[[nodiscard]] std::string ControlRequest_to_json(const ControlRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_request"));
    o.add("request_id", doc.string(v.request_id));
    o.add("request", doc.raw_json(control_request_inner_to_json(v.request)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlRequest, std::string> ControlRequest_from_json(
    std::string_view raw
) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    auto type = root.get("type");
    if (!type.is_str() || type.as_str() != std::string_view("control_request")) {
        return std::unexpected("not a control_request");
    }
    ControlRequest req;
    req.request_id = detail_serde::read_string(root, "request_id");
    auto inner = root.get("request");
    if (!inner.is_obj()) return std::unexpected("request: expected object");
    auto subtype = inner.get("subtype");
    if (!subtype.is_str()) return std::unexpected("subtype: expected string");
    req.request = parse_control_request_inner(subtype.as_str(), inner);
    return req;
}

namespace detail_serde {

[[nodiscard]] std::string control_response_inner_to_json(
    const std::variant<ControlSuccessResponse, ControlErrorResponse>& inner
) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, ControlSuccessResponse>)
                return ControlSuccessResponse_to_json(alt);
            else
                return ControlErrorResponse_to_json(alt);
        },
        inner);
}

} // namespace detail_serde

[[nodiscard]] std::string ControlResponse_to_json(const ControlResponse& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_response"));
    // session_id: present on bridge→server, absent on server→client.
    if (v.session_id.has_value()) {
        o.add("session_id", doc.string(*v.session_id));
    }
    o.add("response", doc.raw_json(detail_serde::control_response_inner_to_json(v.response)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlResponse, std::string> ControlResponse_from_json(
    std::string_view raw
) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    auto type = root.get("type");
    if (!type.is_str() || type.as_str() != std::string_view("control_response")) {
        return std::unexpected("not a control_response");
    }
    ControlResponse resp;
    resp.session_id = detail_serde::read_optional_string(root, "session_id");
    auto inner = root.get("response");
    if (!inner.is_obj()) return std::unexpected("response: expected object");
    auto subtype = inner.get("subtype");
    if (!subtype.is_str()) return std::unexpected("subtype: expected string");
    if (subtype.as_str() == std::string_view("error")) {
        auto r = ControlErrorResponse_from_json(inner);
        if (!r) return std::unexpected(r.error());
        resp.response = std::move(*r);
    } else {
        auto r = ControlSuccessResponse_from_json(inner);
        if (!r) return std::unexpected(r.error());
        resp.response = std::move(*r);
    }
    return resp;
}

namespace detail_serde {

[[nodiscard]] std::string string_map_to_json(
    const std::unordered_map<std::string, std::string>& m
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    for (const auto& [k, v] : m) o.add(k.c_str(), doc.string(v));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::unordered_map<std::string, std::string> string_map_from_json(
    loom::utils::json::JsonVal o
) {
    using namespace loom::utils::json;
    std::unordered_map<std::string, std::string> out;
    if (o.is_obj()) {
        o.iter_obj([&](JsonVal k, JsonVal v) {
            if (k.is_str() && v.is_str())
                out[std::string(k.as_str())] = std::string(v.as_str());
        });
    }
    return out;
}

[[nodiscard]] std::optional<SDKAssistantMessageError>
sdk_assistant_message_error_from_str(std::string_view s) {
    if (s == "authentication_failed") return SDKAssistantMessageError::AuthenticationFailed;
    if (s == "billing_error") return SDKAssistantMessageError::BillingError;
    if (s == "rate_limit") return SDKAssistantMessageError::RateLimit;
    if (s == "invalid_request") return SDKAssistantMessageError::InvalidRequest;
    if (s == "server_error") return SDKAssistantMessageError::ServerError;
    if (s == "unknown") return SDKAssistantMessageError::Unknown;
    if (s == "max_output_tokens") return SDKAssistantMessageError::MaxOutputTokens;
    return std::nullopt;
}

[[nodiscard]] std::optional<ResultErrorSubtype>
result_error_subtype_from_str(std::string_view s) {
    if (s == "error_during_execution") return ResultErrorSubtype::ErrorDuringExecution;
    if (s == "error_max_turns") return ResultErrorSubtype::ErrorMaxTurns;
    if (s == "error_max_budget_usd") return ResultErrorSubtype::ErrorMaxBudgetUsd;
    if (s == "error_max_structured_output_retries")
        return ResultErrorSubtype::ErrorMaxStructuredOutputRetries;
    return std::nullopt;
}

} // namespace detail_serde

[[nodiscard]] std::string SDKPermissionDenial_to_json(const SDKPermissionDenial& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("tool_name", doc.string(v.tool_name));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    o.add("tool_input", doc.raw_json(detail_serde::string_map_to_json(v.tool_input)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKPermissionDenial, std::string>
SDKPermissionDenial_from_json(loom::utils::json::JsonVal o) {
    SDKPermissionDenial v;
    v.tool_name = detail_serde::read_string(o, "tool_name");
    v.tool_use_id = detail_serde::read_string(o, "tool_use_id");
    v.tool_input = detail_serde::string_map_from_json(o.get("tool_input"));
    return v;
}

[[nodiscard]] std::string SDKUserMessage_to_json(const SDKUserMessage& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("user"));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.is_synthetic) o.add("is_synthetic", doc.boolean(*v.is_synthetic));
    if (v.priority) o.add("priority", doc.string(*v.priority));
    if (v.timestamp) o.add("timestamp", doc.string(*v.timestamp));
    if (v.uuid) o.add("uuid", doc.string(*v.uuid));
    if (v.session_id) o.add("session_id", doc.string(*v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKUserMessage, std::string>
SDKUserMessage_from_json(loom::utils::json::JsonVal o) {
    SDKUserMessage v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.is_synthetic = detail_serde::read_optional_bool(o, "is_synthetic");
    v.priority = detail_serde::read_optional_string(o, "priority");
    v.timestamp = detail_serde::read_optional_string(o, "timestamp");
    v.uuid = detail_serde::read_optional_string(o, "uuid");
    v.session_id = detail_serde::read_optional_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKUserMessage, std::string>
SDKUserMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKUserMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKUserMessageReplay_to_json(const SDKUserMessageReplay& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("user"));
    o.add("replay", doc.boolean(true));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.is_synthetic) o.add("is_synthetic", doc.boolean(*v.is_synthetic));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKUserMessageReplay, std::string>
SDKUserMessageReplay_from_json(loom::utils::json::JsonVal o) {
    SDKUserMessageReplay v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.is_synthetic = detail_serde::read_optional_bool(o, "is_synthetic");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKUserMessageReplay, std::string>
SDKUserMessageReplay_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKUserMessageReplay_from_json(parsed->root());
}

[[nodiscard]] std::string SDKAssistantMessage_to_json(const SDKAssistantMessage& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("assistant"));
    o.add("message", doc.raw_json(detail_serde::string_map_to_json(v.message)));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    if (v.error)
        o.add("error",
              doc.string(std::string(detail_serde::sdk_assistant_message_error_to_str(*v.error))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKAssistantMessage, std::string>
SDKAssistantMessage_from_json(loom::utils::json::JsonVal o) {
    SDKAssistantMessage v;
    v.message = detail_serde::string_map_from_json(o.get("message"));
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    if (auto e = detail_serde::read_optional_string(o, "error"))
        v.error = detail_serde::sdk_assistant_message_error_from_str(*e);
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKAssistantMessage, std::string>
SDKAssistantMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKAssistantMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKResultSuccess_to_json(const SDKResultSuccess& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("result"));
    o.add("subtype", doc.string("success"));
    o.add("duration_ms", doc.number(v.duration_ms));
    o.add("duration_api_ms", doc.number(v.duration_api_ms));
    o.add("is_error", doc.boolean(v.is_error));
    o.add("num_turns", doc.number(static_cast<int64_t>(v.num_turns)));
    o.add("result", doc.string(v.result));
    // stop_reason is required-nullable — always emit.
    if (v.stop_reason) o.add("stop_reason", doc.string(*v.stop_reason));
    else o.add("stop_reason", doc.null());
    o.add("total_cost_usd", doc.number(v.total_cost_usd));
    o.add("usage", doc.raw_json(v.usage_json));
    auto mu = doc.object();
    for (const auto& [k, u] : v.model_usage)
        mu.add(k.c_str(), doc.raw_json(ModelUsage_to_json(u)));
    o.add("modelUsage", std::move(mu));
    // permission_denials is required — always emit, even
    // empty (the live emitters always emit "permission_denials":[]).
    auto pd = doc.array();
    for (const auto& d : v.permission_denials)
        pd.append(doc.raw_json(SDKPermissionDenial_to_json(d)));
    o.add("permission_denials", std::move(pd));
    if (v.structured_output)
        o.add("structured_output", doc.raw_json(*v.structured_output));
    if (v.fast_mode_state)
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKResultSuccess, std::string>
SDKResultSuccess_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    SDKResultSuccess v;
    if (auto d = o.get("duration_ms"); d.is_num()) v.duration_ms = d.as_double();
    if (auto d = o.get("duration_api_ms"); d.is_num()) v.duration_api_ms = d.as_double();
    if (auto e = o.get("is_error"); e.is_bool()) v.is_error = e.as_bool();
    if (auto n = o.get("num_turns"); n.is_num()) v.num_turns = static_cast<int>(n.as_int());
    v.result = detail_serde::read_string(o, "result");
    v.stop_reason = detail_serde::read_optional_string(o, "stop_reason");
    if (auto c = o.get("total_cost_usd"); c.is_num()) v.total_cost_usd = c.as_double();
    if (auto u = o.get("usage"); u.valid()) v.usage_json = u.to_string();
    if (auto mu = o.get("modelUsage"); mu.is_obj()) {
        mu.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto u = ModelUsage_from_json(val.to_string());
            if (u) v.model_usage[std::string(k.as_str())] = std::move(*u);
        });
    }
    if (auto pd = o.get("permission_denials"); pd.is_arr()) {
        pd.iter([&](JsonVal el) {
            auto d = SDKPermissionDenial_from_json(el);
            if (d) v.permission_denials.push_back(std::move(*d));
        });
    }
    if (auto so = o.get("structured_output"); so.valid())
        v.structured_output = so.to_string();
    if (auto fms = o.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKResultSuccess, std::string>
SDKResultSuccess_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKResultSuccess_from_json(parsed->root());
}

[[nodiscard]] std::string SDKResultError_to_json(const SDKResultError& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("result"));
    o.add("subtype",
          doc.string(std::string(detail_serde::result_error_subtype_to_str(v.subtype))));
    o.add("duration_ms", doc.number(v.duration_ms));
    o.add("duration_api_ms", doc.number(v.duration_api_ms));
    o.add("is_error", doc.boolean(v.is_error));
    o.add("num_turns", doc.number(static_cast<int64_t>(v.num_turns)));
    // stop_reason is required-nullable — always emit.
    if (v.stop_reason) o.add("stop_reason", doc.string(*v.stop_reason));
    else o.add("stop_reason", doc.null());
    o.add("total_cost_usd", doc.number(v.total_cost_usd));
    o.add("usage", doc.raw_json(v.usage_json));
    auto mu = doc.object();
    for (const auto& [k, u] : v.model_usage)
        mu.add(k.c_str(), doc.raw_json(ModelUsage_to_json(u)));
    o.add("modelUsage", std::move(mu));
    // permission_denials / errors are required — always
    // emit, even empty (the live emitters always emit them).
    auto pd = doc.array();
    for (const auto& d : v.permission_denials)
        pd.append(doc.raw_json(SDKPermissionDenial_to_json(d)));
    o.add("permission_denials", std::move(pd));
    auto errs = doc.array();
    for (const auto& e : v.errors) errs.append(doc.string(e));
    o.add("errors", std::move(errs));
    if (v.fast_mode_state)
        o.add("fast_mode_state",
              doc.string(std::string(detail_serde::fast_mode_state_to_str(*v.fast_mode_state))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKResultError, std::string>
SDKResultError_from_json(loom::utils::json::JsonVal o) {
    using namespace loom::utils::json;
    SDKResultError v;
    if (auto st = o.get("subtype"); st.is_str()) {
        if (auto s = detail_serde::result_error_subtype_from_str(st.as_str()))
            v.subtype = *s;
    }
    if (auto d = o.get("duration_ms"); d.is_num()) v.duration_ms = d.as_double();
    if (auto d = o.get("duration_api_ms"); d.is_num()) v.duration_api_ms = d.as_double();
    if (auto e = o.get("is_error"); e.is_bool()) v.is_error = e.as_bool();
    if (auto n = o.get("num_turns"); n.is_num()) v.num_turns = static_cast<int>(n.as_int());
    v.stop_reason = detail_serde::read_optional_string(o, "stop_reason");
    if (auto c = o.get("total_cost_usd"); c.is_num()) v.total_cost_usd = c.as_double();
    if (auto u = o.get("usage"); u.valid()) v.usage_json = u.to_string();
    if (auto mu = o.get("modelUsage"); mu.is_obj()) {
        mu.iter_obj([&](JsonVal k, JsonVal val) {
            if (!k.is_str()) return;
            auto u = ModelUsage_from_json(val.to_string());
            if (u) v.model_usage[std::string(k.as_str())] = std::move(*u);
        });
    }
    if (auto pd = o.get("permission_denials"); pd.is_arr()) {
        pd.iter([&](JsonVal el) {
            auto d = SDKPermissionDenial_from_json(el);
            if (d) v.permission_denials.push_back(std::move(*d));
        });
    }
    if (auto errs = o.get("errors"); errs.is_arr()) {
        errs.iter([&](JsonVal el) {
            if (el.is_str()) v.errors.emplace_back(el.as_str());
        });
    }
    if (auto fms = o.get("fast_mode_state"); fms.is_str()) {
        if (auto s = detail_serde::fast_mode_state_from_str(fms.as_str()))
            v.fast_mode_state = *s;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKResultError, std::string>
SDKResultError_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKResultError_from_json(parsed->root());
}

[[nodiscard]] std::string SDKSystemMessage_to_json(const SDKSystemMessage& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string(v.subtype));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    if (v.model) o.add("model", doc.string(*v.model));
    if (v.permission_mode)
        o.add("permission_mode",
              doc.string(std::string(detail_serde::permission_mode_to_str(*v.permission_mode))));
    if (v.tools) {
        auto arr = doc.array();
        for (const auto& t : *v.tools) arr.append(doc.string(t));
        o.add("tools", std::move(arr));
    }
    if (v.agents) {
        auto arr = doc.array();
        for (const auto& a : *v.agents) arr.append(doc.string(a));
        o.add("agents", std::move(arr));
    }
    if (v.cwd) o.add("cwd", doc.string(*v.cwd));
    if (v.output_style) o.add("output_style", doc.string(*v.output_style));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKSystemMessage, std::string>
SDKSystemMessage_from_json(loom::utils::json::JsonVal o) {
    SDKSystemMessage v;
    v.subtype = detail_serde::read_string(o, "subtype");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.model = detail_serde::read_optional_string(o, "model");
    if (auto pm = detail_serde::read_optional_string(o, "permission_mode")) {
        if (auto m = detail_serde::permission_mode_from_str(*pm))
            v.permission_mode = *m;
    }
    v.tools = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("tools");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](loom::utils::json::JsonVal el) {
            if (el.is_str()) out.emplace_back(el.as_str());
        });
        return out;
    }();
    v.agents = [&]() -> std::optional<std::vector<std::string>> {
        auto arr = o.get("agents");
        if (!arr.is_arr()) return std::nullopt;
        std::vector<std::string> out;
        arr.iter([&](loom::utils::json::JsonVal el) {
            if (el.is_str()) out.emplace_back(el.as_str());
        });
        return out;
    }();
    v.cwd = detail_serde::read_optional_string(o, "cwd");
    v.output_style = detail_serde::read_optional_string(o, "output_style");
    return v;
}

[[nodiscard]] std::expected<SDKSystemMessage, std::string>
SDKSystemMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKSystemMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKPartialAssistantMessage_to_json(
    const SDKPartialAssistantMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("stream_event"));
    o.add("event", doc.raw_json(v.event_json.empty() ? "{}" : v.event_json));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKPartialAssistantMessage, std::string>
SDKPartialAssistantMessage_from_json(loom::utils::json::JsonVal o) {
    SDKPartialAssistantMessage v;
    if (auto e = o.get("event"); e.valid()) v.event_json = e.to_string();
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKPartialAssistantMessage, std::string>
SDKPartialAssistantMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKPartialAssistantMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKCompactBoundaryMessage_to_json(
    const SDKCompactBoundaryMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("compact_boundary"));
    o.add("trigger", doc.string(v.trigger));
    o.add("pre_tokens", doc.number(static_cast<int64_t>(v.pre_tokens)));
    if (v.preserved_segment) {
        auto ps = doc.object();
        ps.add("head_uuid", doc.string(v.preserved_segment->head_uuid));
        ps.add("anchor_uuid", doc.string(v.preserved_segment->anchor_uuid));
        ps.add("tail_uuid", doc.string(v.preserved_segment->tail_uuid));
        o.add("preserved_segment", std::move(ps));
    }
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKCompactBoundaryMessage, std::string>
SDKCompactBoundaryMessage_from_json(loom::utils::json::JsonVal o) {
    SDKCompactBoundaryMessage v;
    v.trigger = detail_serde::read_string(o, "trigger");
    if (auto pt = o.get("pre_tokens"); pt.is_num())
        v.pre_tokens = static_cast<int>(pt.as_int());
    if (auto ps = o.get("preserved_segment"); ps.is_obj()) {
        SDKCompactBoundaryMessage::PreservedSegment seg;
        seg.head_uuid = detail_serde::read_string(ps, "head_uuid");
        seg.anchor_uuid = detail_serde::read_string(ps, "anchor_uuid");
        seg.tail_uuid = detail_serde::read_string(ps, "tail_uuid");
        v.preserved_segment = std::move(seg);
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKCompactBoundaryMessage, std::string>
SDKCompactBoundaryMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKCompactBoundaryMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKStatusMessage_to_json(const SDKStatusMessage& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("status"));
    if (v.status) o.add("status", doc.string(*v.status));
    if (v.permission_mode)
        o.add("permission_mode",
              doc.string(std::string(detail_serde::permission_mode_to_str(*v.permission_mode))));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKStatusMessage, std::string>
SDKStatusMessage_from_json(loom::utils::json::JsonVal o) {
    SDKStatusMessage v;
    v.status = detail_serde::read_optional_string(o, "status");
    if (auto pm = detail_serde::read_optional_string(o, "permission_mode")) {
        if (auto m = detail_serde::permission_mode_from_str(*pm))
            v.permission_mode = *m;
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKStatusMessage, std::string>
SDKStatusMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStatusMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKToolProgressMessage_to_json(
    const SDKToolProgressMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("tool_progress"));
    o.add("tool_use_id", doc.string(v.tool_use_id));
    o.add("tool_name", doc.string(v.tool_name));
    if (v.parent_tool_use_id)
        o.add("parent_tool_use_id", doc.string(*v.parent_tool_use_id));
    o.add("elapsed_time_seconds", doc.number(v.elapsed_time_seconds));
    if (v.task_id) o.add("task_id", doc.string(*v.task_id));
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKToolProgressMessage, std::string>
SDKToolProgressMessage_from_json(loom::utils::json::JsonVal o) {
    SDKToolProgressMessage v;
    v.tool_use_id = detail_serde::read_string(o, "tool_use_id");
    v.tool_name = detail_serde::read_string(o, "tool_name");
    v.parent_tool_use_id = detail_serde::read_optional_string(o, "parent_tool_use_id");
    if (auto e = o.get("elapsed_time_seconds"); e.is_num())
        v.elapsed_time_seconds = e.as_double();
    v.task_id = detail_serde::read_optional_string(o, "task_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKToolProgressMessage, std::string>
SDKToolProgressMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKToolProgressMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKPostTurnSummaryMessage_to_json(
    const SDKPostTurnSummaryMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("system"));
    o.add("subtype", doc.string("post_turn_summary"));
    o.add("summarizes_uuid", doc.string(v.summarizes_uuid));
    o.add("status_category", doc.string(v.status_category));
    o.add("status_detail", doc.string(v.status_detail));
    o.add("is_noteworthy", doc.boolean(v.is_noteworthy));
    o.add("title", doc.string(v.title));
    o.add("description", doc.string(v.description));
    o.add("recent_action", doc.string(v.recent_action));
    o.add("needs_action", doc.string(v.needs_action));
    if (!v.artifact_urls.empty()) {
        auto arr = doc.array();
        for (const auto& u : v.artifact_urls) arr.append(doc.string(u));
        o.add("artifact_urls", std::move(arr));
    }
    o.add("uuid", doc.string(v.uuid));
    o.add("session_id", doc.string(v.session_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKPostTurnSummaryMessage, std::string>
SDKPostTurnSummaryMessage_from_json(loom::utils::json::JsonVal o) {
    SDKPostTurnSummaryMessage v;
    v.summarizes_uuid = detail_serde::read_string(o, "summarizes_uuid");
    v.status_category = detail_serde::read_string(o, "status_category");
    v.status_detail = detail_serde::read_string(o, "status_detail");
    if (auto n = o.get("is_noteworthy"); n.is_bool()) v.is_noteworthy = n.as_bool();
    v.title = detail_serde::read_string(o, "title");
    v.description = detail_serde::read_string(o, "description");
    v.recent_action = detail_serde::read_string(o, "recent_action");
    v.needs_action = detail_serde::read_string(o, "needs_action");
    if (auto arr = o.get("artifact_urls"); arr.is_arr()) {
        arr.iter([&](loom::utils::json::JsonVal el) {
            if (el.is_str()) v.artifact_urls.emplace_back(el.as_str());
        });
    }
    v.uuid = detail_serde::read_string(o, "uuid");
    v.session_id = detail_serde::read_string(o, "session_id");
    return v;
}

[[nodiscard]] std::expected<SDKPostTurnSummaryMessage, std::string>
SDKPostTurnSummaryMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKPostTurnSummaryMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKStreamlinedTextMessage_to_json(
    const SDKStreamlinedTextMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("streamlined_text"));
    o.add("text", doc.string(v.text));
    o.add("session_id", doc.string(v.session_id));
    o.add("uuid", doc.string(v.uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKStreamlinedTextMessage, std::string>
SDKStreamlinedTextMessage_from_json(loom::utils::json::JsonVal o) {
    SDKStreamlinedTextMessage v;
    v.text = detail_serde::read_string(o, "text");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    return v;
}

[[nodiscard]] std::expected<SDKStreamlinedTextMessage, std::string>
SDKStreamlinedTextMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStreamlinedTextMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKStreamlinedToolUseSummaryMessage_to_json(
    const SDKStreamlinedToolUseSummaryMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("streamlined_tool_use_summary"));
    o.add("tool_summary", doc.string(v.tool_summary));
    o.add("session_id", doc.string(v.session_id));
    o.add("uuid", doc.string(v.uuid));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<SDKStreamlinedToolUseSummaryMessage, std::string>
SDKStreamlinedToolUseSummaryMessage_from_json(loom::utils::json::JsonVal o) {
    SDKStreamlinedToolUseSummaryMessage v;
    v.tool_summary = detail_serde::read_string(o, "tool_summary");
    v.session_id = detail_serde::read_string(o, "session_id");
    v.uuid = detail_serde::read_string(o, "uuid");
    return v;
}

[[nodiscard]] std::expected<SDKStreamlinedToolUseSummaryMessage, std::string>
SDKStreamlinedToolUseSummaryMessage_from_json(std::string_view raw) {
    auto parsed = loom::utils::json::parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    return SDKStreamlinedToolUseSummaryMessage_from_json(parsed->root());
}

[[nodiscard]] std::string SDKMessage_to_json(const SDKMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKAssistantMessage>)
                return SDKAssistantMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKUserMessage>)
                return SDKUserMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKUserMessageReplay>)
                return SDKUserMessageReplay_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKResultSuccess>)
                return SDKResultSuccess_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKResultError>)
                return SDKResultError_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKSystemMessage>)
                return SDKSystemMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKPartialAssistantMessage>)
                return SDKPartialAssistantMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKCompactBoundaryMessage>)
                return SDKCompactBoundaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStatusMessage>)
                return SDKStatusMessage_to_json(alt);
            else
                return SDKToolProgressMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] std::expected<SDKMessage, std::string>
SDKMessage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("SDKMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("SDKMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "user") {
        auto replay = root.get("replay");
        if (replay.is_bool() && replay.as_bool()) {
            auto r = SDKUserMessageReplay_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
        } else {
            auto r = SDKUserMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
        }
        return std::unexpected("SDKMessage: failed to parse user message");
    }
    if (t == "assistant") {
        auto r = SDKAssistantMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "result") {
        auto st = root.get("subtype");
        if (st.is_str() && st.as_str() == std::string_view("success")) {
            auto r = SDKResultSuccess_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        auto r = SDKResultError_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "system") {
        auto st = root.get("subtype");
        const auto subtype = st.is_str() ? std::string(st.as_str()) : std::string{};
        if (subtype == "compact_boundary") {
            auto r = SDKCompactBoundaryMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        if (subtype == "status") {
            auto r = SDKStatusMessage_from_json(root);
            if (r) return SDKMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
        auto r = SDKSystemMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "stream_event") {
        auto r = SDKPartialAssistantMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "tool_progress") {
        auto r = SDKToolProgressMessage_from_json(root);
        if (r) return SDKMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    return std::unexpected("SDKMessage: unknown type: " + t);
}

[[nodiscard]] std::string ControlCancelRequest_to_json(const ControlCancelRequest& v) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("control_cancel"));
    o.add("request_id", doc.string(v.request_id));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<ControlCancelRequest, std::string>
ControlCancelRequest_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    ControlCancelRequest v;
    v.request_id = detail_serde::read_string(root, "request_id");
    return v;
}

[[nodiscard]] std::string KeepAliveMessage_to_json(const KeepAliveMessage&) {
    return R"({"type":"keep_alive"})";
}

[[nodiscard]] std::expected<KeepAliveMessage, std::string>
KeepAliveMessage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    if (!parsed->root().is_obj()) return std::unexpected("expected object");
    return KeepAliveMessage{};
}

[[nodiscard]] std::string UpdateEnvironmentVariablesMessage_to_json(
    const UpdateEnvironmentVariablesMessage& v
) {
    using namespace loom::utils::json;
    JsonMutDoc doc;
    auto o = doc.object();
    o.add("type", doc.string("update_environment_variables"));
    o.add("variables", doc.raw_json(detail_serde::string_map_to_json(v.variables)));
    doc.set_root(std::move(o));
    return doc.to_string();
}

[[nodiscard]] std::expected<UpdateEnvironmentVariablesMessage, std::string>
UpdateEnvironmentVariablesMessage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("expected object");
    UpdateEnvironmentVariablesMessage v;
    v.variables = detail_serde::string_map_from_json(root.get("variables"));
    return v;
}

[[nodiscard]] std::string StdoutMessage_to_json(const StdoutMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKMessage>)
                return SDKMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStreamlinedTextMessage>)
                return SDKStreamlinedTextMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKStreamlinedToolUseSummaryMessage>)
                return SDKStreamlinedToolUseSummaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, SDKPostTurnSummaryMessage>)
                return SDKPostTurnSummaryMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlResponse>)
                return ControlResponse_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRequest>)
                return ControlRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlCancelRequest>)
                return ControlCancelRequest_to_json(alt);
            else
                return KeepAliveMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] std::expected<StdoutMessage, std::string>
StdoutMessage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("StdoutMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("StdoutMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "streamlined_text") {
        auto r = SDKStreamlinedTextMessage_from_json(root);
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "streamlined_tool_use_summary") {
        auto r = SDKStreamlinedToolUseSummaryMessage_from_json(root);
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_response") {
        auto r = ControlResponse_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_request") {
        auto r = ControlRequest_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_cancel") {
        auto r = ControlCancelRequest_from_json(root.to_string());
        if (r) return StdoutMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "keep_alive") {
        return StdoutMessage{KeepAliveMessage{}};
    }
    if (t == "system") {
        // post_turn_summary is a direct StdoutMessage alternative; the rest
        // of the system/* family goes through SDKMessage.
        auto st = root.get("subtype");
        if (st.is_str() && st.as_str() == std::string_view("post_turn_summary")) {
            auto r = SDKPostTurnSummaryMessage_from_json(root);
            if (r) return StdoutMessage{std::move(*r)};
            return std::unexpected(r.error());
        }
    }
    // Everything else (user/assistant/result/system/stream_event/tool_progress)
    // is an SDKMessage.
    auto r = SDKMessage_from_json(raw);
    if (r) return StdoutMessage{std::move(*r)};
    return std::unexpected(r.error());
}

[[nodiscard]] std::string StdinMessage_to_json(const StdinMessage& v) {
    return std::visit(
        []<typename T>(const T& alt) -> std::string {
            if constexpr (std::is_same_v<T, SDKUserMessage>)
                return SDKUserMessage_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlRequest>)
                return ControlRequest_to_json(alt);
            else if constexpr (std::is_same_v<T, ControlResponse>)
                return ControlResponse_to_json(alt);
            else if constexpr (std::is_same_v<T, KeepAliveMessage>)
                return KeepAliveMessage_to_json(alt);
            else
                return UpdateEnvironmentVariablesMessage_to_json(alt);
        },
        v);
}

[[nodiscard]] std::expected<StdinMessage, std::string>
StdinMessage_from_json(std::string_view raw) {
    using namespace loom::utils::json;
    auto parsed = parse(raw);
    if (!parsed) return std::unexpected(parsed.error().message());
    auto root = parsed->root();
    if (!root.is_obj()) return std::unexpected("StdinMessage: expected object");
    auto type = root.get("type");
    if (!type.is_str()) return std::unexpected("StdinMessage: type: expected string");
    const auto t = std::string(type.as_str());
    if (t == "user") {
        auto r = SDKUserMessage_from_json(root);
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_request") {
        auto r = ControlRequest_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "control_response") {
        auto r = ControlResponse_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    if (t == "keep_alive") {
        return StdinMessage{KeepAliveMessage{}};
    }
    if (t == "update_environment_variables") {
        auto r = UpdateEnvironmentVariablesMessage_from_json(root.to_string());
        if (r) return StdinMessage{std::move(*r)};
        return std::unexpected(r.error());
    }
    return std::unexpected("StdinMessage: unknown type: " + t);
}


} // namespace loom::server::control