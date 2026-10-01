// Implementation unit for cc.orchestration.runtime_backends — the lifted
// 'lsp' runtime tool backend. The parse/format/execute bodies moved
// verbatim from runtime_registry_executors.cpp (RFC-0001 B15); the
// 11-string parse_lsp_action mirror is intentionally duplicated here next
// to lsp_tool.cppm's lsp_action_name() (the documented silent-wrong-answer
// hazard: the two must move together and stay byte-identical).
module;

module loom.orchestration.runtime_backends;

import std;

import loom.types.tool_types;
import loom.orchestration.tools.lsp;
import loom.tools.runtime_registry;

namespace loom::orchestration::detail {

using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

namespace fs = std::filesystem;

[[nodiscard]] loom::tools::LspAction parse_lsp_action(std::string_view action) {
    // Canonical action strings mirror lsp_action_name() in lsp_tool.cppm.
    // Without these mappings the runtime registry's lsp backend would
    // silently fall through to LspAction::Symbols for the newer actions.
    if (action == "diagnostics") return loom::tools::LspAction::Diagnostics;
    if (action == "definition") return loom::tools::LspAction::Definition;
    if (action == "references") return loom::tools::LspAction::References;
    if (action == "completion") return loom::tools::LspAction::Completion;
    if (action == "hover") return loom::tools::LspAction::Hover;
    if (action == "symbols") return loom::tools::LspAction::Symbols;
    if (action == "implementation") return loom::tools::LspAction::Implementation;
    if (action == "workspaceSymbol") return loom::tools::LspAction::WorkspaceSymbol;
    if (action == "prepareCallHierarchy") return loom::tools::LspAction::PrepareCallHierarchy;
    if (action == "incomingCalls") return loom::tools::LspAction::IncomingCalls;
    if (action == "outgoingCalls") return loom::tools::LspAction::OutgoingCalls;
    return loom::tools::LspAction::Symbols;
}

[[nodiscard]] std::string format_lsp_result(const loom::tools::LspResult& result, std::string_view action) {
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
    auto file_text = loom::tools::detail::json_string(json, "file_path")
        .or_else([&] { return loom::tools::detail::json_string(json, "path"); });
    auto action = loom::tools::detail::json_string(json, "action").value_or("symbols");
    if (!file_text || file_text->empty()) {
        return ToolResult::error("lsp requires file_path");
    }
    fs::path file = *file_text;
    if (!fs::exists(file)) {
        return ToolResult::error(std::format("File not found: {}", file.string()));
    }

    loom::tools::LspTool tool;
    tool.set_connected(true);
    loom::tools::LspRequest request{
        .action = parse_lsp_action(action),
        .file_path = file,
        .position = loom::tools::LspPosition{
            .line = loom::tools::detail::json_int(json, "line").value_or(0),
            .character = loom::tools::detail::json_int(json, "character").value_or(0)},
        .query = loom::tools::detail::json_string(json, "query"),
    };
    auto result = tool.execute(std::move(request));
    if (!result) return ToolResult::error(std::string(loom::tools::format_error(result.error())));
    return ToolResult::success(format_lsp_result(*result, action));
}

} // namespace loom::orchestration::detail
