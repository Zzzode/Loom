/// @file core_settings_loader.cppm
/// @brief RFC-0001 B4 composition root: bridges the core settings layer
/// (loom::core::ConfigManager) into the native MCP runtime through the loader
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

export module loom.commands.mcp.core_settings_loader;

import std;

import loom.config.config;
import loom.orchestration.tools.mcp;
import loom.services.mcp.xaa_idp_login;

export namespace loom::commands {

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
    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            loom::core::ConfigManager config;
            auto loaded = config.load();
            if (!loaded) return std::unexpected(loaded.error().message);

            loom::tools::CoreSettingsMcpLayer layer;
            for (const auto& server : config.settings().mcp_servers) {
                layer.servers.push_back(loom::tools::to_native_mcp_server(server));
            }
            // settings.xaaIdp is the SINGLE store for this port: `/mcp xaa
            // setup --callback-port` writes it and `/mcp xaa login` already
            // reads it via read_xaa_idp_status(); the --xaa runtime path now
            // consumes the same field instead of the removed
            // ~/.loom/xaa-idp.txt `callback_port` key.
            layer.xaa_callback_port = config.settings().xaa_idp.callback_port;

            // RFC-0001 followup c20: the IdP client secret's single store is
            // the hardened ~/.config/loom/xaa/idp_tokens.json (written by
            // `/mcp xaa setup --client-secret`), keyed by the SAME settings
            // issuer `/mcp xaa setup --issuer` writes. The --xaa runtime path
            // now consumes it here instead of the dead hand-edited
            // ~/.loom/xaa-idp.txt `idp_client_secret` line. When the issuer is
            // empty (XAA not configured) get_idp_client_secret("") misses, so
            // the field stays unset — no special-casing on the read path.
            layer.xaa_idp_client_secret =
                loom::services::mcp::get_idp_client_secret(
                    config.settings().xaa_idp.issuer);

            // One-time migration of the legacy hand-edited line. GUARDED on a
            // non-empty settings issuer: with an empty issuer (XAA not
            // configured, or after `/mcp xaa clear` reset settings.xaaIdp to
            // {}) save_idp_client_secret would write under the phantom key
            // mcpXaaIdpConfig."".clientSecret, after which
            // get_idp_client_secret("") no longer misses and the secret would
            // resurface on the next XAA auth — silently undoing the clear.
            // Self-disabling: once migrated, the store has the secret and this
            // branch is never reached again (reload_from_config() re-runs the
            // loader but finds the store populated). The legacy file is never
            // modified or deleted.
            if (!layer.xaa_idp_client_secret &&
                !config.settings().xaa_idp.issuer.empty()) {
                auto legacy = loom::services::mcp::read_legacy_idp_client_secret();
                if (legacy) {
                    loom::services::mcp::save_idp_client_secret(
                        config.settings().xaa_idp.issuer, *legacy);
                    layer.xaa_idp_client_secret = *legacy;
                }
            }
            return layer;
        });
}

}  // namespace loom::commands
