/// @file agent_display.cppm
/// @brief Pure-function data preparation for rendering agent information.
///
/// This module only contains data-structure definitions and pure
/// computations (sorting, override resolution, model display, labels).
///
/// UI rendering lives in src/ui/agents/ (FTXUI components).
module;

#include <cctype>
#include <cstddef>
#include <cstdint>

export module loom.tools.agent_display;

import std;

import loom.tools.agent_runtime;

export namespace loom::tools::agent_display {

using loom::tools::agent_runtime::AgentDefinition;

// ============================================================================
// Source groups (ordered list used by both CLI and interactive UI)
// ============================================================================

/// Display label + underlying source value for an agent source bucket.
struct AgentSourceGroup {
    std::string label;
    std::string source;  // matches AgentDefinition::source field values
};

/// Ordered list of agent source groups for display.
/// Both the CLI and interactive UI should use this to ensure consistent ordering.
[[nodiscard]] inline std::vector<AgentSourceGroup> agent_source_groups() {
    return {
        {"User agents",        "userSettings"},
        {"Project agents",     "projectSettings"},
        {"Local agents",       "localSettings"},
        {"Managed agents",     "policySettings"},
        {"Plugin agents",      "plugin"},
        {"CLI arg agents",     "flagSettings"},
        {"Built-in agents",    "built-in"},
    };
}

// ============================================================================
// Override resolution
// ============================================================================

/// An AgentDefinition annotated with information about whether it was
/// overridden by a higher-priority source (and, if so, which source won).
struct ResolvedAgent {
    AgentDefinition definition;
    /// If present, this source "won" and therefore `definition` is not active.
    std::optional<std::string> overridden_by;

    // Convenience accessors mirroring TS spread semantics:
    [[nodiscard]] std::string_view agent_type() const noexcept { return definition.agent_type; }
    [[nodiscard]] std::string_view source()     const noexcept { return definition.source; }
};

/// Annotate agents with override information by comparing against the active
/// (winning) agent list.  An agent is "overridden" when another agent with the
/// same type from a higher-priority source takes precedence.
///
/// Also deduplicates by (agentType, source) to handle git worktree duplicates
/// where the same agent file is loaded from both the worktree and main repo.
[[nodiscard]] inline std::vector<ResolvedAgent> resolve_agent_overrides(
    const std::vector<AgentDefinition>& all_agents,
    const std::vector<AgentDefinition>& active_agents
) {
    std::unordered_map<std::string, const AgentDefinition*> active_map;
    active_map.reserve(active_agents.size());
    for (const auto& agent : active_agents) {
        active_map[agent.agent_type] = &agent;
    }

    std::set<std::string> seen;
    std::vector<ResolvedAgent> resolved;
    resolved.reserve(all_agents.size());

    for (const auto& agent : all_agents) {
        const std::string key = agent.agent_type + ":" + agent.source;
        if (!seen.insert(key).second) continue;  // worktree duplicate

        const char* overridden_by = nullptr;
        auto it = active_map.find(agent.agent_type);
        if (it != active_map.end() && it->second->source != agent.source) {
            overridden_by = it->second->source.c_str();
        }

        resolved.push_back(ResolvedAgent{
            .definition = agent,
            .overridden_by = overridden_by ? std::optional<std::string>{overridden_by}
                                           : std::nullopt,
        });
    }

    return resolved;
}

// ============================================================================
// Model / source display helpers
// ============================================================================

/// Resolve the display model string for an agent.
/// Returns the model alias or 'inherit' for display purposes.
///
/// The optional `global_default` parameter lets callers plug in the real
/// runtime value without this module taking a direct dependency on bootstrap.
[[nodiscard]] inline std::optional<std::string> resolve_agent_model_display(
    const AgentDefinition& agent,
    std::optional<std::string_view> global_default = std::nullopt
) {
    std::string_view model;
    if (!agent.model.empty()) {
        model = agent.model;
    } else if (global_default && !global_default->empty()) {
        model = *global_default;
    } else {
        return std::nullopt;
    }
    if (model.empty()) return std::nullopt;
    return std::string{model};
}

/// Get a short capitalised display name for a setting source (matches TS
/// `getSourceDisplayName`).
[[nodiscard]] inline std::string_view source_display_name(std::string_view source) {
    if (source == "userSettings")    return "User";
    if (source == "projectSettings") return "Project";
    if (source == "localSettings")   return "Local";
    if (source == "policySettings")  return "Managed";
    if (source == "flagSettings")    return "Flag";
    if (source == "plugin")          return "Plugin";
    if (source == "built-in")        return "Built-in";
    return "Unknown";
}

/// Get a human-readable label for the source that overrides an agent.
/// Returns lowercase, e.g. "user", "project", "managed" — matches TS
/// `getOverrideSourceLabel`.
[[nodiscard]] inline std::string override_source_label(std::string_view source) {
    auto name = std::string{source_display_name(source)};
    std::ranges::transform(name, name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}

// ============================================================================
// Sorting / listing
// ============================================================================

/// Case-insensitive alphabetical comparison on `agent_type`.
/// Usable with `std::sort`, `std::ranges::sort`, etc.
struct CompareAgentsByName {
    [[nodiscard]] bool operator()(const AgentDefinition& a, const AgentDefinition& b) const {
        const auto& lhs = a.agent_type;
        const auto& rhs = b.agent_type;
        const auto min_len = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i < min_len; ++i) {
            const unsigned char ca = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(lhs[i])));
            const unsigned char cb = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(rhs[i])));
            if (ca != cb) return ca < cb;
        }
        return lhs.size() < rhs.size();
    }
};

/// Convenience wrapper that returns a sorted copy.
[[nodiscard]] inline std::vector<AgentDefinition> sort_by_name(std::vector<AgentDefinition> agents) {
    std::ranges::sort(agents, CompareAgentsByName{});
    return agents;
}

} // namespace loom::tools::agent_display
