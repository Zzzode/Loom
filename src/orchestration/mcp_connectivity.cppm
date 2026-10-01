/// @file mcp_connectivity.cppm
/// @brief RFC-0001 B7/B8 MCP-connectivity bridge.
///
/// Consumes the B6 NativeMcpRuntime snapshot sink
/// (loom::tools::set_mcp_snapshots_sink) and projects every emitted snapshot
/// vector into the EXISTING cc.hooks.remaining_notifs MCP connectivity slot
/// via set_raw_mcp_connectivity, so the current get_mcp_connectivity_status()
/// / has_mcp_connectivity_issues() readers see the data unchanged.
///
/// SOLE FEED (since the B8 atomic cut): the old direct projection
/// (inject_mcp_connectivity_from_manager inside
/// NativeMcpRuntime::all_statuses) and the hook-local mapper/services
/// imports were deleted. The sink installed here is now the ONLY writer of
/// the hook connectivity slot — the hook module itself is pure data/slot
/// with zero cc.services imports.
///
/// Single-sink design: one std::function installed exactly once at the
/// composition root (main()). The hook gains no provider/refresh API —
/// set_raw_mcp_connectivity is already the slot setter the notif readers
/// consume.
module;

#include <cstdint>

export module loom.orchestration.mcp_connectivity;

// graph_check parser hazard: qualify cc.tools symbols WITHOUT a leading "::"
// (write loom::tools::set_mcp_snapshots_sink, never ::loom::tools::...). A
// leading-colon chain defeats the namespace-path evidence heuristic and
// makes this import look like a NEW dead import. Un-prefixed loom:: qualifiers
// are used consistently throughout this file.
import std;

import loom.hooks.remaining_notifs;
import loom.services.mcp.types;
import loom.services.mcp.connection_manager;
import loom.orchestration.tools.mcp;

export namespace loom::orchestration::mcp_connectivity {

// The 5-arm ConnectionStatus -> McpServerStatus mapping. The hook-local
// copy (to_mcp_server_status) was deleted with the B8 cut; this is the only
// mapping.
inline auto to_hook_status(loom::services::mcp::ConnectionStatus s)
    -> loom::hooks::notifs::McpServerStatus {
    using CS = loom::services::mcp::ConnectionStatus;
    switch (s) {
        case CS::Connected:    return loom::hooks::notifs::McpServerStatus::Connected;
        case CS::Connecting:   return loom::hooks::notifs::McpServerStatus::Connecting;
        case CS::NeedsAuth:    return loom::hooks::notifs::McpServerStatus::Error;   // surfaced for auth nudge
        case CS::Error:        return loom::hooks::notifs::McpServerStatus::Error;
        case CS::Disconnected:
        default:               return loom::hooks::notifs::McpServerStatus::Disconnected;
    }
}

// Projection body: both ids copy snap.name, last_error is copied, and every
// row shares one now-ms stamp. The clock is the hook module's OWN exported
// detail::now_ms(), the same clock the deleted direct leg used to read.
inline auto project_connectivity(
    std::vector<loom::services::mcp::McpServerSnapshot> snapshots
) -> std::vector<loom::hooks::notifs::McpConnectivityInfo> {
    std::vector<loom::hooks::notifs::McpConnectivityInfo> infos;
    infos.reserve(snapshots.size());
    const int64_t now = loom::hooks::notifs::detail::now_ms();
    for (const auto& snap : snapshots) {
        loom::hooks::notifs::McpConnectivityInfo info;
        info.server_id    = snap.name;
        info.display_name = snap.name;
        info.state        = to_hook_status(snap.status);
        info.last_error   = snap.last_error;
        info.last_seen_ms = now;
        infos.push_back(std::move(info));
    }
    return infos;
}

// Install the single bridge sink on the tools runtime. The lambda is
// captureless and must not re-install sinks (it runs on the runtime status
// read path, after NativeMcpRuntime::mutex_ has been released). Idempotent
// setter; call exactly once at the composition root before any status read.
inline void wire_mcp_connectivity() {
    loom::tools::set_mcp_snapshots_sink(
        [](std::vector<loom::services::mcp::McpServerSnapshot> snaps) {
            loom::hooks::notifs::set_raw_mcp_connectivity(
                project_connectivity(std::move(snaps)));
        });
}

} // export namespace loom::orchestration::mcp_connectivity
