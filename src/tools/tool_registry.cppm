// ToolRegistry - Re-exports core ToolRegistry and provides tool list factory
module;


export module loom.tools.registry;

import std;

import loom.tools.tool;
import loom.tools.runtime_registry;

export namespace loom::tools::registry {

// Re-export core ToolRegistry for convenience
using loom::core::ToolRegistry;
using loom::core::ToolDefinition;
using loom::core::ToolInput;
using loom::core::ToolResult;
using loom::core::ITool;

/// Get the list of all built-in tool names
[[nodiscard]] inline std::vector<std::string> builtin_tool_names() {
    return loom::tools::runtime_tool_names();
}

} // namespace loom::tools::registry
