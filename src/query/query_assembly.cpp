// Implementation unit for loom.query.assembly — the engine assembly recipe
// extracted from the server's detail::execute_native_query (RFC 0001
// cc-sdk phase 3, §2.1). Bodies live here (not the interface) so the
// orchestration/runtime-registry imports never enter the module BMI.
module;

#include <cstdlib>

module loom.query.assembly;

import std;

import loom.types.types;
import loom.config.config;
import loom.query.query_engine;
import loom.tools.tool;
import loom.tools.runtime_registry;
import loom.tools.agent_types;
import loom.hooks.tool_permissions;
import loom.orchestration.runtime_backends;
import loom.serdes.json;

namespace loom::query {

namespace {

namespace fs = std::filesystem;

// Moved verbatim from server_routes.cppm:686-702 (the
// AgentLivePermissionCheck <-> ToolPermissionHook bridge). The assembly
// wires this as the registry's permission_check when the caller supplies an
// ask_user callback but no explicit permission_check, so the re-expressed
// server route keeps its current permission behaviour.
[[nodiscard]] loom::tools::AgentLivePermissionCheck check_agent_tool_permission(
    loom::hooks::ToolPermissionHook& permission_hook,
    std::string_view tool_name,
    std::string_view input_json,
    std::string_view tool_use_id
) {
    permission_hook.set_current_tool_use_id(tool_use_id);
    auto response = permission_hook.can_use_response(tool_name, input_json);
    permission_hook.clear_current_tool_use_id();

    loom::tools::AgentLivePermissionCheck check;
    check.allowed = response.decision == loom::hooks::PermissionDecision::allow ||
                    response.decision == loom::hooks::PermissionDecision::allow_once;
    check.updated_input_json = std::move(response.updated_input_json);
    check.message = std::move(response.message);
    return check;
}

// ── Content-block parsing helpers (session resume) ─────────────────────
// The JSONL format (written by QueryEngine::append_message_to_json) stores
// content as either a plain string (single text block) or an array of
// typed block objects: text / thinking / tool_use / tool_result / image /
// document.  The resume parser must round-trip all of them — the original
// implementation only read content-as-string and silently dropped every
// multi-block message (thinking + tool_use chains vanished on resume).

/// Parse one content-block JSON object into a ContentBlock.
/// Returns nullopt for unknown types (skipped, not fatal).
[[nodiscard]] std::optional<loom::core::ContentBlock> parse_content_block(
    loom::utils::json::JsonVal block)
{
    if (!block.is_obj()) return std::nullopt;
    auto type_val = block.get("type");
    if (!type_val.is_str()) return std::nullopt;
    const auto type = std::string(type_val.as_str());

    if (type == "text") {
        auto tv = block.get("text");
        if (!tv.is_str()) return std::nullopt;
        return loom::core::TextBlock{std::string(tv.as_str())};
    }
    if (type == "thinking") {
        auto tv = block.get("thinking");
        if (!tv.is_str()) return std::nullopt;
        auto sv = block.get("signature");
        return loom::core::ThinkingBlock{
            .thinking  = std::string(tv.as_str()),
            .signature = sv.is_str() ? std::string(sv.as_str()) : ""};
    }
    if (type == "tool_use") {
        auto id_val   = block.get("id");
        auto name_val = block.get("name");
        if (!id_val.is_str() || !name_val.is_str()) return std::nullopt;
        // input is a JSON object; re-serialize to the input_json string.
        std::string input_json;
        if (auto iv = block.get("input"); iv.valid()) {
            input_json = iv.to_string();
        }
        return loom::core::ToolUseBlock{
            .id         = loom::core::ToolUseId{std::string(id_val.as_str())},
            .name       = std::string(name_val.as_str()),
            .input_json = std::move(input_json)};
    }
    if (type == "image" || type == "document") {
        auto src = block.get("source");
        if (!src.is_obj()) return std::nullopt;
        auto mv = src.get("media_type");
        auto dv = src.get("data");
        if (!mv.is_str() || !dv.is_str()) return std::nullopt;
        if (type == "image") {
            loom::core::ImageBlock img;
            img.media_type = std::string(mv.as_str());
            img.data       = std::string(dv.as_str());
            return img;
        }
        return loom::core::DocumentBlock{
            .media_type = std::string(mv.as_str()),
            .data       = std::string(dv.as_str())};
    }
    return std::nullopt;  // unknown block type — skip
}

/// Parse the `content` field of a tool_result object (string or array of
/// text/image items) into ContentBlocks.
[[nodiscard]] std::vector<loom::core::ContentBlock> parse_tool_result_content(
    loom::utils::json::JsonVal cv)
{
    std::vector<loom::core::ContentBlock> blocks;
    if (cv.is_str()) {
        blocks.push_back(loom::core::TextBlock{std::string(cv.as_str())});
        return blocks;
    }
    if (!cv.is_arr()) return blocks;
    for (std::size_t i = 0; i < cv.size(); ++i) {
        auto item = cv.at(i);
        if (!item.is_obj()) continue;
        auto tv = item.get("type");
        if (!tv.is_str()) continue;
        const auto t = std::string(tv.as_str());
        if (t == "text") {
            auto txt = item.get("text");
            if (txt.is_str())
                blocks.push_back(loom::core::TextBlock{std::string(txt.as_str())});
        } else if (t == "image") {
            auto src = item.get("source");
            if (src.is_obj()) {
                auto mv = src.get("media_type");
                auto dv = src.get("data");
                if (mv.is_str() && dv.is_str()) {
                    loom::core::ImageBlock img;
                    img.media_type = std::string(mv.as_str());
                    img.data       = std::string(dv.as_str());
                    blocks.push_back(std::move(img));
                }
            }
        }
    }
    return blocks;
}

} // namespace

// Rank-10 seed-line parser. The server's prior_message_lines are the
// session messages.jsonl lines written by message_json()
// (server_routes.cppm:141): {"id","role","content"(string),"created_at",...}.
// This is the same shape session_line_to_message parses (server_routes.cppm
// :299), re-homed here so a rank-10 assembly need not call the rank-13
// server helper. The seed is fed to QueryEngine::restore_conversation (the
// real resume path that rebuilds content-replacement state), not the lossy
// append_message_for_testing-based server helper.
//
// Exported (declared in query_assembly.cppm) so the harness's resume() path
// can parse session messages without a loom.server import. Defined here in
// namespace loom::query (NOT the anonymous namespace above) so the exported
// declaration links.
//
// content format: a plain string (single text block) OR an array of typed
// block objects (text / thinking / tool_use / tool_result / image /
// document).  ToolResultMessage is serialized as role="user" with a
// tool_result object as the first array element.
[[nodiscard]] std::optional<loom::core::Message> parse_session_message_value(
    loom::utils::json::JsonVal root,
    std::size_t index
) {
    if (!root.valid() || !root.is_obj()) return std::nullopt;
    auto role_val = root.get("role");
    auto content_val = root.get("content");
    if (!role_val.is_str() || !content_val.valid()) return std::nullopt;

    const auto role = std::string(role_val.as_str());

    std::string id;
    auto id_val = root.get("id");
    if (id_val.valid() && id_val.is_str()) {
        id = std::string(id_val.as_str());
    } else {
        id = "restored_" + std::to_string(index);
    }
    const auto timestamp = std::chrono::system_clock::now();

    // ── content as string: single text block ──
    if (content_val.is_str()) {
        const auto content = std::string(content_val.as_str());
        if (role == "assistant") {
            loom::core::AssistantMessage msg{};
            msg.id.value = id;
            msg.timestamp = timestamp;
            msg.content.push_back(loom::core::TextBlock{content});
            auto model = root.get("model");
            if (model.valid() && model.is_str())
                msg.model = std::string(model.as_str());
            return loom::core::Message{std::move(msg)};
        }
        if (role == "user" || role == "system") {
            loom::core::UserMessage msg{};
            msg.id.value = id;
            msg.timestamp = timestamp;
            msg.content.push_back(loom::core::TextBlock{content});
            return loom::core::Message{std::move(msg)};
        }
        return std::nullopt;
    }

    // ── content as array: full content blocks ──
    if (!content_val.is_arr() || content_val.size() == 0) return std::nullopt;

    // ToolResultMessage is serialized as role="user" with a tool_result
    // object as the first element of the content array.
    if (role == "user") {
        auto first = content_val.at(0);
        if (first.is_obj()) {
            auto ftype = first.get("type");
            if (ftype.is_str() &&
                std::string(ftype.as_str()) == "tool_result")
            {
                loom::core::ToolResultMessage trm{};
                trm.id.value = id;
                trm.timestamp = timestamp;
                if (auto tv = first.get("tool_use_id"); tv.is_str())
                    trm.tool_use_id =
                        loom::core::ToolUseId{std::string(tv.as_str())};
                if (auto ev = first.get("is_error"); ev.is_bool())
                    trm.is_error = ev.as_bool();
                if (auto cv = first.get("content"); cv.valid())
                    trm.content = parse_tool_result_content(cv);
                // Document blocks may follow the tool_result object.
                for (std::size_t i = 1; i < content_val.size(); ++i) {
                    if (auto blk = parse_content_block(content_val.at(i)))
                        trm.content.push_back(std::move(*blk));
                }
                return loom::core::Message{std::move(trm)};
            }
        }
    }

    // Regular content blocks (thinking, tool_use, text, image, document).
    std::vector<loom::core::ContentBlock> blocks;
    for (std::size_t i = 0; i < content_val.size(); ++i) {
        if (auto blk = parse_content_block(content_val.at(i)))
            blocks.push_back(std::move(*blk));
    }
    if (blocks.empty()) return std::nullopt;

    if (role == "assistant") {
        loom::core::AssistantMessage msg{};
        msg.id.value = id;
        msg.timestamp = timestamp;
        msg.content = std::move(blocks);
        auto model = root.get("model");
        if (model.valid() && model.is_str())
            msg.model = std::string(model.as_str());
        return loom::core::Message{std::move(msg)};
    }
    if (role == "user" || role == "system") {
        loom::core::UserMessage msg{};
        msg.id.value = id;
        msg.timestamp = timestamp;
        msg.content = std::move(blocks);
        return loom::core::Message{std::move(msg)};
    }

    return std::nullopt;
}

namespace {

// Internal seed-line wrapper: parse the JSON line, then delegate to the
// exported parse_session_message_value. Used by assemble()'s
// prior_message_lines path.
[[nodiscard]] std::optional<loom::core::Message> parse_seed_message(
    const std::string& line,
    std::size_t index
) {
    auto parsed = loom::utils::json::parse(line);
    if (!parsed) return std::nullopt;
    return parse_session_message_value(parsed->root(), index);
}

} // namespace

// ============================================================
// AssemblyHandle — PIMPL special members
// ============================================================

struct AssemblyHandle::Impl {
    // Declaration/construction order is load-bearing (§2.3): hook ->
    // registry -> engine. The registry's RuntimeFunctionTools store the
    // permission_check lambda capturing &permission_hook, and the engine
    // invokes the tools; reverse declaration order guarantees reverse
    // destruction (engine dies first, then registry, then hook).
    std::unique_ptr<loom::hooks::ToolPermissionHook> permission_hook;
    std::unique_ptr<loom::core::ToolRegistry> registry;
    std::unique_ptr<loom::core::QueryEngine> engine;
};

AssemblyHandle::AssemblyHandle(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

AssemblyHandle::AssemblyHandle(AssemblyHandle&&) noexcept = default;
AssemblyHandle& AssemblyHandle::operator=(AssemblyHandle&&) noexcept = default;
AssemblyHandle::~AssemblyHandle() = default;

loom::core::QueryEngine& AssemblyHandle::engine() noexcept {
    return *impl_->engine;
}

// ============================================================
// resolve_engine_config — the recipe's :714-731 settings mapping
// ============================================================

loom::core::Result<loom::core::QueryEngineConfig> resolve_engine_config(
    const loom::core::Settings& settings,
    const AssemblyOverrides& overrides
) {
    loom::core::QueryEngineConfig config;
    config.api_key = overrides.api_key.value_or(settings.network.api_key.value_or(""));
    if (overrides.base_url) {
        config.base_url = *overrides.base_url;
    } else if (settings.network.base_url) {
        config.base_url = *settings.network.base_url;
    }
    config.model_params.model =
        overrides.requested_model.value_or(settings.model.default_model);
    config.model_params.max_tokens = settings.model.max_output_tokens;
    config.model_params.temperature = settings.model.temperature;
    config.context_window.max_context_tokens = settings.model.context_window_size;
    config.retry_policy.max_retries = settings.network.max_retries;
    config.thinking_config.mode = settings.model.extended_thinking
        ? loom::core::ThinkingConfig::Mode::Adaptive
        : loom::core::ThinkingConfig::Mode::Disabled;
    config.thinking_config.budget_tokens = settings.model.thinking_budget;
    config.cwd = overrides.cwd.value_or(fs::current_path().string());
    // permissions.deny must be enforced on the headless/server engine too,
    // not only the interactive CLI — otherwise denied tools still reach
    // the model through this path. Same single source (ConfigManager).
    config.always_deny_rules = settings.permissions.deny_rules;
    if (overrides.wire_api) {
        config.wire_api = *overrides.wire_api;
    }
    return config;
}

// ============================================================
// assemble — the assemble-only recipe
// ============================================================

loom::core::Result<AssemblyHandle> assemble(
    const AssemblyConfig& config,
    const AssemblyCallbacks& callbacks
) {
    auto impl = std::make_unique<AssemblyHandle::Impl>();

    // Step 4 (RFC-0001 B11): ensure orchestration runtime backends (image
    // codec this batch) are installed before this per-session ToolRegistry
    // can dispatch Read/computer_use. std::call_once-guarded: repeat calls
    // (and the loom main() install in the same process) are no-ops.
    loom::orchestration::install_runtime_backends();

    // Step 3: the permission hook. Always constructed (stable address for
    // the registry lambdas and the engine pointer) but only configured and
    // wired when the caller supplies an ask_user bridge — an unconfigured
    // hook would default every check to ask_user and deny tool execution.
    impl->permission_hook = std::make_unique<loom::hooks::ToolPermissionHook>();
    const bool have_ask_user = callbacks.ask_user.has_value();
    if (have_ask_user) {
        impl->permission_hook->set_auto_approve(false);
        if (config.engine.cwd) {
            impl->permission_hook->set_working_dir(*config.engine.cwd);
        } else {
            impl->permission_hook->set_working_dir(fs::current_path().string());
        }
        impl->permission_hook->set_ask_user_response_fn(*callbacks.ask_user);
    }

    // Step 5: ToolRegistry + runtime tools.
    impl->registry = std::make_unique<loom::core::ToolRegistry>();
    loom::tools::RuntimeToolOptions options;
    options.parent_permission_mode = std::nullopt;
    if (callbacks.permission_check) {
        options.permission_check = *callbacks.permission_check;
    } else if (have_ask_user) {
        // Bridge the hook to the registry's AgentLivePermissionCheckFn so
        // the re-expressed server route keeps its current behaviour (the
        // server wired check_agent_tool_permission(permission_hook, ...)).
        options.permission_check = loom::tools::AgentLivePermissionCheckFn{
            [hook = impl->permission_hook.get()](
                std::string_view tool_name,
                std::string_view input_json,
                std::string_view tool_use_id
            ) {
                return check_agent_tool_permission(
                    *hook, tool_name, input_json, tool_use_id);
            }};
    }
    options.permission_hook_valid_for_background = false;
    loom::tools::register_runtime_tools(*impl->registry, std::move(options));

    // Test seam: register extra tools before the config.tools snapshot.
    if (config.register_extra_tools) {
        config.register_extra_tools(*impl->registry);
    }

    // Step 6: route unregistered tool names (e.g. MCP server tools) to
    // connected MCP servers (RFC-0001 B15 unified fallback).
    impl->registry->set_missing_tool_handler(
        loom::orchestration::make_missing_tool_backend());

    // Step 7 + 8: snapshot visible definitions and wire the dynamic MCP
    // tool providers (recipe steps 7-8, server_routes.cppm:800-810) so MCP
    // tool discovery is preserved.
    auto engine_config = config.engine;
    engine_config.tools = impl->registry->get_visible_definitions();
    engine_config.dynamic_tools_provider = []() -> std::vector<loom::core::ToolDefinition> {
        return loom::tools::collect_mcp_tool_definitions();
    };
    engine_config.mcp_input_schema_provider = [] {
        return loom::tools::collect_mcp_input_schemas();
    };

    // Step 9: the engine. Constructed AFTER the tool snapshot (config.tools
    // is consumed at query time, so the engine must see the registered
    // tools — §2.3 body-construction rationale).
    impl->engine = std::make_unique<loom::core::QueryEngine>(
        std::move(engine_config), *impl->registry);

    // Step 10: abort / permission callbacks.
    if (config.cancel_flag) {
        impl->engine->set_external_abort_callback([flag = config.cancel_flag] {
            return flag->load();
        });
    }
    if (have_ask_user) {
        impl->engine->set_permission_hook(impl->permission_hook.get());
    }

    // Optional session storage / prompt dump (the server route passes
    // sessions_dir; the harness may pass either).
    if (config.sessions_dir) {
        impl->engine->set_session_storage(*config.sessions_dir);
    }
    if (config.dump_prompts_dir) {
        impl->engine->set_dump_prompts_dir(*config.dump_prompts_dir);
    }

    // Step 11: seed prior messages via the real resume path.
    if (!config.prior_message_lines.empty()) {
        std::vector<loom::core::Message> messages;
        messages.reserve(config.prior_message_lines.size());
        for (std::size_t index = 0; index < config.prior_message_lines.size(); ++index) {
            if (auto msg = parse_seed_message(config.prior_message_lines[index], index)) {
                messages.push_back(std::move(*msg));
            }
        }
        if (!messages.empty()) {
            impl->engine->restore_conversation(std::move(messages));
        }
    }

    return AssemblyHandle(std::move(impl));
}

} // namespace loom::query
