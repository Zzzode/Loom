// Implementation unit for cc.orchestration.runtime_backends — the lifted
// 'lsp' runtime tool backend. The parse/format/execute bodies moved
// verbatim from runtime_registry_executors.cpp (RFC-0001 B15); the
// 11-string parse_lsp_action mirror is intentionally duplicated here next
// to lsp_tool.cppm's lsp_action_name() (the documented silent-wrong-answer
// hazard: the two must move together and stay byte-identical).
module;

module cc.orchestration.runtime_backends;

import std;

import cc.types.tool_types;
import cc.orchestration.tools.lsp;
import cc.tools.runtime_registry;

namespace cc::orchestration::detail {

using cc::core::Result;
using cc::core::ToolInput;
using cc::core::ToolResult;

namespace fs = std::filesystem;

[[nodiscard]] cc::tools::LspAction parse_lsp_action(std::string_view action) {
    // Canonical action strings mirror lsp_action_name() in lsp_tool.cppm.
    // Without these mappings the runtime registry's lsp backend would
    // silently fall through to LspAction::Symbols for the newer actions.
    if (action == "diagnostics") return cc::tools::LspAction::Diagnostics;
    if (action == "definition") return cc::tools::LspAction::Definition;
    if (action == "references") return cc::tools::LspAction::References;
    if (action == "completion") return cc::tools::LspAction::Completion;
    if (action == "hover") return cc::tools::LspAction::Hover;
    if (action == "symbols") return cc::tools::LspAction::Symbols;
    if (action == "implementation") return cc::tools::LspAction::Implementation;
    if (action == "workspaceSymbol") return cc::tools::LspAction::WorkspaceSymbol;
    if (action == "prepareCallHierarchy") return cc::tools::LspAction::PrepareCallHierarchy;
    if (action == "incomingCalls") return cc::tools::LspAction::IncomingCalls;
    if (action == "outgoingCalls") return cc::tools::LspAction::OutgoingCalls;
    return cc::tools::LspAction::Symbols;
}

[[nodiscard]] std::string format_lsp_result(const cc::tools::LspResult& result, std::string_view action) {
    if (result.empty()) return std::format("No LSP results for action '{}'.", action);
    std::string out;
    for (const auto& diagnostic : result.diagnostics) {
        out += std::format("{}:{}:{} {}\n", diagnostic.source,
            diagnostic.range.start.line, diagnostic.range.start.character, diagnostic.message);
    }
    for (const auto& location : result.locations) {
        out += std::format("{}:{}:{}\n", location.uri, location.range.start.line, location.range.start.character);
    }
    for (const auto& completion : result.completions) {
        out += std::format("{} {}\n", completion.label, completion.detail);
    }
    for (const auto& symbol : result.symbols) {
        out += std::format("{} {}\n", symbol.kind, symbol.name);
    }
    if (result.hover) out += result.hover->contents;
    return out.empty() ? std::format("No LSP results for action '{}'.", action) : out;
}

[[nodiscard]] Result<ToolResult> lsp_backend(const ToolInput& input) {
    auto json = input.json();
    auto file_text = cc::tools::detail::json_string(json, "file_path")
        .or_else([&] { return cc::tools::detail::json_string(json, "path"); });
    auto action = cc::tools::detail::json_string(json, "action").value_or("symbols");
    if (!file_text || file_text->empty()) {
        return ToolResult::error("lsp requires file_path");
    }
    fs::path file = *file_text;
    if (!fs::exists(file)) {
        return ToolResult::error(std::format("File not found: {}", file.string()));
    }

    cc::tools::LspTool tool;
    tool.set_connected(true);
    cc::tools::LspRequest request{
        .action = parse_lsp_action(action),
        .file_path = file,
        .position = cc::tools::LspPosition{
            .line = cc::tools::detail::json_int(json, "line").value_or(0),
            .character = cc::tools::detail::json_int(json, "character").value_or(0)},
        .query = cc::tools::detail::json_string(json, "query"),
    };
    auto result = tool.execute(std::move(request));
    if (!result) return ToolResult::error(std::string(cc::tools::format_error(result.error())));
    return ToolResult::success(format_lsp_result(*result, action));
}

} // namespace cc::orchestration::detail
