/// @file mcp_cmd.cpp
/// @brief Module implementation unit for cc.commands.mcp_cmd. Out-of-line
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

namespace cc::commands {

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
                              std::size_t start, bool allow_global) {
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
        if (!scope ||
            (!allow_global && *scope == McpStorageScope::Global)) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest,
                std::format("Invalid scope '{}'. Use local | user | project{}",
                            label, allow_global ? " | global" : "")));
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
            "Usage: /mcp enable|disable <name|all> [--scope local|user|project|global]");
    }
    if (auto loaded = ensure_config_loaded(); !loaded) {
        return std::unexpected(loaded.error());
    }

    const std::string name = args[1];
    const bool all = (name == "all");
    auto parsed_scope = extract_scope_arg(args, 2, /*allow_global=*/true);
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

    const auto& merged = config_manager_.settings().mcp_servers;
    std::map<McpStorageScope, std::vector<std::string>> names_by_scope;
    std::map<McpStorageScope, std::filesystem::path> path_by_scope;
    for (const auto& [scope, path] : config_manager_.mcp_scope_paths()) {
        path_by_scope[scope] = path;
    }

    if (all) {
        for (const auto& server : merged) {
            if (server.disabled.value_or(false) == disabled) continue;
            auto owner = config_manager_.mcp_server_owner(server.name);
            if (!owner) continue;
            names_by_scope[*owner].push_back(server.name);
        }
        if (names_by_scope.empty()) {
            return CommandResult::success(disabled
                ? std::string("All MCP servers are already disabled")
                : std::string("All MCP servers are already enabled"));
        }
    } else {
        auto it = std::ranges::find_if(merged,
            [&](const auto& s) { return s.name == name; });
        if (it == merged.end()) {
            return CommandResult::fail(
                std::format("MCP server '{}' not found", name));
        }
        if (*parsed_scope) {
            // Explicit scope: the name must physically be in that file.
            auto files = config_manager_.find_mcp_server_files(name);
            bool present = std::ranges::any_of(files,
                [&](const auto& f) { return f.first == **parsed_scope; });
            if (!present) {
                return CommandResult::fail(std::format(
                    "MCP server '{}' not found in the '{}' scope",
                    name, scope_label));
            }
            names_by_scope[**parsed_scope].push_back(name);
        } else {
            // Preserve the idempotent "already enabled/disabled" wording.
            if (it->disabled.value_or(false) == disabled) {
                return CommandResult::success(disabled
                    ? std::format("MCP server '{}' is already disabled", name)
                    : std::format("MCP server '{}' is already enabled", name));
            }
            auto owner = config_manager_.mcp_server_owner(name);
            if (!owner) {
                return CommandResult::fail(
                    std::format("MCP server '{}' not found", name));
            }
            names_by_scope[*owner].push_back(name);
        }
    }

    // Patch each distinct owner file once; collect file-level failures.
    std::vector<std::filesystem::path> failed_files;
    std::size_t changed = 0;
    for (const auto& [scope, names] : names_by_scope) {
        auto result = config_manager_.set_mcp_servers_disabled_in(
            scope, std::span<const std::string>(names), disabled);
        if (result) {
            changed += names.size();
        } else if (auto found = path_by_scope.find(scope);
                   found != path_by_scope.end()) {
            failed_files.push_back(found->second);
        }
    }

    if (changed == 0) {
        std::string paths;
        for (std::size_t i = 0; i < failed_files.size(); ++i) {
            if (i) paths += ", ";
            paths += failed_files[i].string();
        }
        return CommandResult::fail(std::format(
            "Could not {} MCP server(s); failed to write: {}",
            disabled ? "disable" : "enable", paths));
    }

    if (auto synced = reload_and_sync(); !synced) {
        return std::unexpected(synced.error());
    }

    std::string out;
    if (all) {
        out = std::format(
            "{} {} MCP server(s); takes effect on next start",
            disabled ? "Disabled" : "Enabled", changed);
    } else {
        out = std::format(
            "MCP server '{}' {}; takes effect on next start",
            name, disabled ? "disabled" : "enabled");
    }
    if (!failed_files.empty()) {
        out += "\nWarning: some files could not be written:";
        for (const auto& path : failed_files) {
            out += std::format("\n  {}", path.string());
        }
    }
    return CommandResult::success(std::move(out));
}

}  // namespace cc::commands
