/// @file mcp_cmd.cpp
/// @brief Module implementation unit for loom.commands.mcp_cmd. Out-of-line
/// bodies for the per-tier --scope routing helpers (RFC-0001 B followup c6);
/// the inline-def ratchet keeps the interface unit at its frozen body count.
module;

#include <cstddef>
#include <cstdint>
#include <cstdlib>

module loom.commands.mcp_cmd;

import std;

import loom.types.types;
import loom.commands.command;
import loom.config.config;

namespace loom::commands {

[[nodiscard]] VoidResult McpCommand::reload_and_sync() {
    // c23: drop the latch so the post-mutation reload is unconditional — the
    // caller just patched a tier file and needs the in-memory snapshot
    // refreshed before syncing the native runtime. (The patched file's
    // signature also differs, so tier_files_changed() would reload anyway,
    // but clearing the latch makes the force explicit and load-order-proof.)
    config_loaded_ = false;
    if (auto loaded = ensure_config_loaded(); !loaded) return loaded;
    return sync_native_runtime();
}

[[nodiscard]] Result<std::optional<McpStorageScope>>
McpCommand::extract_scope_arg(std::span<const std::string> args,
                              std::size_t start) {
    // LAST occurrence wins, matching the value the add loop keeps and the
    // value echoed in error messages.
    std::optional<McpStorageScope> resolved;
    for (std::size_t i = start; i < args.size(); ++i) {
        if (args[i] != "--scope" && args[i] != "-s") continue;
        if (i + 1 >= args.size()) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest, "--scope requires a value"));
        }
        const std::string_view label = args[i + 1];
        auto scope = ConfigManager::mcp_scope_from_label(label);
        if (!scope) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest,
                std::format("Invalid scope '{}'. Use local | user | project",
                            label)));
        }
        resolved = *scope;
    }
    return resolved;
}

[[nodiscard]] Result<CommandResult>
McpCommand::execute_set_disabled(std::span<const std::string> args,
                                 bool disabled) {
    if (args.size() < 2) {
        return CommandResult::fail(
            "Usage: /mcp enable|disable <name|all> [--scope local|user|project]");
    }
    if (auto loaded = ensure_config_loaded(); !loaded) {
        return std::unexpected(loaded.error());
    }

    const std::string name = args[1];
    const bool all = (name == "all");
    auto parsed_scope = extract_scope_arg(args, 2);
    if (!parsed_scope) {
        return std::unexpected(parsed_scope.error());
    }
    std::string scope_label;
    for (std::size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i] == "--scope" || args[i] == "-s") {
            scope_label = args[i + 1];
        }
    }
    if (all && *parsed_scope) {
        return CommandResult::fail("'all' cannot be combined with --scope");
    }

    // Delegate to ConfigManager — the fallback-aware outcome model handles
    // read-only skips, idempotent no-ops, and per-entry status.
    loom::core::McpMutationOutcome outcome;
    if (all) {
        outcome = config_manager_.set_all_mcp_servers_disabled(disabled);
    } else {
        outcome = config_manager_.set_mcp_server_disabled(
            name, disabled, *parsed_scope);
    }

    // Categorize entries.
    std::size_t modified = 0;
    std::size_t unchanged = 0;
    std::size_t skipped = 0;
    std::vector<std::string> failed_names;
    std::vector<std::string> skipped_names;
    std::vector<std::string> not_found_names;
    for (const auto& entry : outcome.entries) {
        switch (entry.status) {
            using enum loom::core::McpMutationStatus;
            case Modified:        ++modified; break;
            case Unchanged:       ++unchanged; break;
            case SkippedReadOnly:
                ++skipped;
                skipped_names.push_back(entry.server_name);
                break;
            case WriteFailed:
                failed_names.push_back(entry.server_name);
                break;
            case NotFound:
                not_found_names.push_back(entry.server_name);
                break;
        }
    }

    auto join_str = [](const std::vector<std::string>& items) {
        std::string joined;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) joined += ", ";
            joined += items[i];
        }
        return joined;
    };

    // ── Render ──────────────────────────────────────────────────────────

    // Named server, not found.
    if (!all && modified == 0 && unchanged == 0 && skipped == 0 &&
        failed_names.empty() && !not_found_names.empty()) {
        if (!scope_label.empty()) {
            return CommandResult::fail(std::format(
                "MCP server '{}' not found in the '{}' scope",
                name, scope_label));
        }
        return CommandResult::fail(
            std::format("MCP server '{}' not found", name));
    }

    // All servers, empty config.
    if (all && outcome.is_empty()) {
        return CommandResult::success("No MCP servers configured");
    }

    // All servers, all already in target state (no fallback entries).
    if (all && modified == 0 && skipped == 0 && failed_names.empty() &&
        unchanged > 0) {
        return CommandResult::success(disabled
            ? std::string("All MCP servers are already disabled")
            : std::string("All MCP servers are already enabled"));
    }

    // Named server, already in target state.
    if (!all && modified == 0 && skipped == 0 && failed_names.empty() &&
        unchanged > 0) {
        return CommandResult::success(disabled
            ? std::format("MCP server '{}' is already disabled", name)
            : std::format("MCP server '{}' is already enabled", name));
    }

    // All entries are read-only fallback.
    if (modified == 0 && unchanged == 0 && failed_names.empty() &&
        skipped > 0) {
        return CommandResult::fail(std::format(
            "All MCP servers are in a read-only fallback config. "
            "Use 'mcp add --scope user' to create writable shadows, "
            "then {} them. Skipped: {}",
            disabled ? "disable" : "enable",
            join_str(skipped_names)));
    }

    // Only reload/sync when at least one file actually changed.
    if (modified > 0) {
        if (auto synced = reload_and_sync(); !synced) {
            return std::unexpected(synced.error());
        }
    }

    // Build the result message.
    std::string out;
    if (all) {
        out = std::format(
            "{} {} MCP server(s); takes effect on next start",
            disabled ? "Disabled" : "Enabled", modified);
    } else {
        out = std::format(
            "MCP server '{}' {}; takes effect on next start",
            name, disabled ? "disabled" : "enabled");
    }

    if (!skipped_names.empty()) {
        out += std::format(
            "\nSkipped (read-only fallback): {}",
            join_str(skipped_names));
    }
    if (!failed_names.empty()) {
        out += std::format(
            "\nWarning: could not write: {}",
            join_str(failed_names));
    }

    // If nothing was modified, it's a failure — even if some entries were
    // skipped (read-only fallback) or had write failures.
    if (modified == 0) {
        return CommandResult::fail(std::move(out));
    }
    return CommandResult::success(std::move(out));
}

}  // namespace loom::commands
