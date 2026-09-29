/// @file core_settings_loader.cppm
/// @brief RFC-0001 B4 composition root: bridges the core settings layer
/// (cc::core::ConfigManager) into the native MCP runtime through the loader
/// sink declared in cc.orchestration.tools.mcp. This module is the ONLY place allowed to
/// know both cc.config.config and cc.orchestration.tools.mcp, which deletes the
/// cc.orchestration.tools.mcp -> cc.config.config upward edge (tools rank 8 -> config rank 1).
///
/// The lambda body below is the verbatim ConfigManager block that used to
/// live in NativeMcpRuntime::ensure_loaded_from_config. The production binary
/// calls install_core_settings_mcp_loader() once from main(), before any path
/// can reach the native runtime; test binaries that need the core layer must
/// install it explicitly and reset the slot to null in cleanup.
module;

export module cc.commands.mcp.core_settings_loader;

import std;

import cc.config.config;
import cc.orchestration.tools.mcp;

export namespace cc::commands {

/// Install the production core-settings MCP loader into the cc.orchestration.tools.mcp
/// sink. Idempotent: installing again simply replaces the previous loader.
/// The loader is invoked lazily on the native runtime's first config load and
/// must propagate a failed ConfigManager::load() verbatim.
///
/// RFC-0001 B followup c17a: the loader ALSO returns the configured XAA IdP
/// callback port (settings.xaaIdp.callbackPort) in the same CoreSettingsMcpLayer.
/// This module is the one composition root that may see both cc.config.config
/// and cc.orchestration.tools.mcp, so it is where the single authoritative port
/// store is read into the runtime — the XAA `--xaa` login path then forwards it
/// to authenticate_xaa() without either layer importing the other.
inline void install_core_settings_mcp_loader() {
    cc::tools::set_core_settings_mcp_loader(
        []() -> std::expected<cc::tools::CoreSettingsMcpLayer, std::string> {
            cc::core::ConfigManager config;
            auto loaded = config.load();
            if (!loaded) return std::unexpected(loaded.error().message);

            cc::tools::CoreSettingsMcpLayer layer;
            for (const auto& server : config.settings().mcp_servers) {
                layer.servers.push_back(cc::tools::to_native_mcp_server(server));
            }
            // settings.xaaIdp is the SINGLE store for this port: `/mcp xaa
            // setup --callback-port` writes it and `/mcp xaa login` already
            // reads it via read_xaa_idp_status(); the --xaa runtime path now
            // consumes the same field instead of the removed
            // ~/.loom/xaa-idp.txt `callback_port` key.
            layer.xaa_callback_port = config.settings().xaa_idp.callback_port;
            return layer;
        });
}

}  // namespace cc::commands
