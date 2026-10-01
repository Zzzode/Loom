// Implementation unit for cc.orchestration.runtime_backends — the four
// lifted MCP runtime tool backends (mcp / list_mcp_resources /
// read_mcp_resource / mcp_auth) and the two MCP snapshot-derived providers
// (visible tool definitions and verbatim input schemas). Bodies lifted
// verbatim from runtime_registry_dispatch.cpp and
// runtime_registry_register.cpp (RFC-0001 B15); each backend derives its
// own input.json() — no shared state with the tools-side dispatcher.
module;

module loom.orchestration.runtime_backends;

import std;

import loom.types.tool_types;
import loom.orchestration.tools.mcp;
import loom.tools.runtime_registry;

namespace loom::orchestration::detail {

using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

[[nodiscard]] Result<ToolResult> list_mcp_resources_backend(const ToolInput& input) {
    auto json = input.json();
    auto server = loom::tools::detail::json_string(json, "server_name")
        .or_else([&] { return loom::tools::detail::json_string(json, "server"); });
    loom::tools::ListMcpResourcesTool tool;
    auto resources = tool.execute(server);
    if (!resources) return ToolResult::error(std::string(loom::tools::format_error(resources.error())));
    std::string out = "MCP resources:\n";
    for (const auto& resource : *resources) {
        out += std::format("- {} ({})\n", resource.uri, resource.mime_type);
    }
    if (resources->empty()) out += "No MCP resources are registered.\n";
    return ToolResult::success(out);
}

[[nodiscard]] Result<ToolResult> read_mcp_resource_backend(const ToolInput& input) {
    auto json = input.json();
    auto server = loom::tools::detail::json_string(json, "server_name")
        .or_else([&] { return loom::tools::detail::json_string(json, "server"); });
    auto uri = loom::tools::detail::json_string(json, "resource_uri")
        .or_else([&] { return loom::tools::detail::json_string(json, "uri"); });
    if (!server || !uri) return ToolResult::error("read_mcp_resource requires server_name and resource_uri");
    loom::tools::ReadMcpResourceTool tool;
    auto result = tool.execute(*server, *uri);
    if (!result) return ToolResult::error(std::string(loom::tools::format_error(result.error())));
    return ToolResult::success(result->content);
}

[[nodiscard]] Result<ToolResult> mcp_backend(const ToolInput& input) {
    auto json = input.json();
    auto server = loom::tools::detail::json_string(json, "server_name")
        .or_else([&] { return loom::tools::detail::json_string(json, "server"); });
    auto tool = loom::tools::detail::json_string(json, "tool_name")
        .or_else([&] { return loom::tools::detail::json_string(json, "tool"); });
    if (!server || !tool) return ToolResult::error("mcp requires server_name and tool_name");
    loom::tools::McpTool mcp_tool;
    auto arguments = loom::tools::detail::json_raw_value(json, "arguments")
        .or_else([&] { return loom::tools::detail::json_raw_value(json, "input"); })
        .value_or("{}");
    auto result = mcp_tool.execute(loom::tools::McpToolRequest{
        .server_name = *server,
        .tool_name = *tool,
        .arguments = {},
        .arguments_json = arguments,
    });
    if (!result) return ToolResult::error(std::string(loom::tools::format_error(result.error())));
    // Preserves structured content items (screenshots, multi-text).
    return loom::tools::mcp_result_to_tool_result(*result);
}

[[nodiscard]] Result<ToolResult> mcp_auth_backend(const ToolInput& input) {
    auto json = input.json();
    auto server = loom::tools::detail::json_string(json, "server_name")
        .or_else([&] { return loom::tools::detail::json_string(json, "server"); });
    if (!server) return ToolResult::error("mcp_auth requires server_name");
    auto code = loom::tools::detail::json_string(json, "auth_code")
        .or_else([&] { return loom::tools::detail::json_string(json, "code"); });
    auto wait_for_callback =
        loom::tools::detail::json_bool(json, "wait_for_callback", false) ||
        loom::tools::detail::json_bool(json, "waitForCallback", false);
    auto authorization_url_file = loom::tools::detail::json_string(json, "authorization_url_file")
        .or_else([&] { return loom::tools::detail::json_string(json, "authorizationUrlFile"); });
    loom::tools::McpAuthTool tool;
    auto result = tool.execute(*server, code, wait_for_callback, authorization_url_file);
    if (!result) return ToolResult::error(std::string(loom::tools::format_error(result.error())));
    return ToolResult::success(*result);
}

} // namespace loom::orchestration::detail

namespace loom::tools {

// ── MCP tool pool for config.tools ────────────────────────────────────────
// TS PARITY: assembleToolPool() merges built-in tools with per-server MCP
// tools so the model can call them directly by short name. Lifted verbatim
// from runtime_registry_register.cpp.
[[nodiscard]] std::vector<loom::core::ToolDefinition> collect_mcp_tool_definitions() {
    std::vector<loom::core::ToolDefinition> defs;
    auto& runtime = loom::tools::NativeMcpRuntime::instance();
    for (const auto& server : runtime.all_statuses()) {
        for (const auto& tool : server.tools) {
            // Skip tools that might collide with built-in names.
            if (tool.name.empty()) continue;
            loom::core::ToolDefinition def;
            def.name = tool.name;
            def.description = tool.description;
            // The simplified property model cannot represent nested MCP
            // input schemas. The verbatim schema is carried by
            // NativeMcpRuntime and surfaced at request serialization time
            // (see QueryEngine's tool serializer); leave the simplified
            // schema empty here.
            def.input_schema = loom::core::InputSchema{};
            def.permission = loom::core::ToolPermission::Network;
            def.is_hidden = false;
            def.category = std::format("mcp:{}", server.name);
            defs.push_back(std::move(def));
        }
    }
    return defs;
}

// Snapshot connected MCP tools' verbatim input schemas keyed by tool name.
// Fed to QueryEngineConfig::mcp_input_schema_provider so the request
// serializer emits the servers' real (possibly nested) JSON schemas rather
// than the empty simplified schema stored on the tool defs.
[[nodiscard]] std::unordered_map<std::string, std::string>
collect_mcp_input_schemas() {
    std::unordered_map<std::string, std::string> schemas;
    for (const auto& server : loom::tools::NativeMcpRuntime::instance().all_statuses()) {
        for (const auto& tool : server.tools) {
            if (!tool.input_schema_json.empty()) {
                schemas.emplace(tool.name, tool.input_schema_json);
            }
        }
    }
    return schemas;
}

} // namespace loom::tools
