// Implementation unit for loom.orchestration.tools.mcp — RFC-0001 B4 core-settings MCP loader
// sink storage. The two bodies live here (not in the .cppm) so the
// inline-definition ratchet on loom.orchestration.tools.mcp does not grow; the single
// function-local static below is the one strong symbol every TU binds to,
// matching the NativeMcpRuntime::instance / global_mcp_router anchor style.
module;

module loom.orchestration.tools.mcp;

import std;

namespace loom::tools::detail {

CoreSettingsMcpServersLoader& core_settings_mcp_loader_slot() {
    static CoreSettingsMcpServersLoader loader;
    return loader;
}

}  // namespace loom::tools::detail

namespace loom::tools {

void set_core_settings_mcp_loader(CoreSettingsMcpServersLoader loader) {
    detail::core_settings_mcp_loader_slot() = std::move(loader);
}

}  // namespace loom::tools
