
export module loom.tools.agent_types;

import std;

export namespace loom::tools {

enum class AgentType {
    Explore,
    Plan,
    Verify,
    GeneralPurpose,
    Custom
};

struct MultiAgentAgentConfig {
    AgentType type;
    std::string name;
    std::string model;
    std::string system_prompt;
    std::vector<std::string> allowed_tools;
    std::optional<int> max_turns;
};

struct MultiAgentResult {
    std::string output;
    int turns_used;
    int tokens_used;
    bool completed;
};

inline auto agent_type_to_string(AgentType type) -> std::string_view {
    switch (type) {
        case AgentType::Explore:        return "explore";
        case AgentType::Plan:           return "plan";
        case AgentType::Verify:         return "verify";
        case AgentType::GeneralPurpose: return "general";
        case AgentType::Custom:         return "custom";
    }
    return "unknown";
}

inline auto parse_agent_type(std::string_view str) -> std::optional<AgentType> {
    if (str == "explore")  return AgentType::Explore;
    if (str == "plan")     return AgentType::Plan;
    if (str == "verify")   return AgentType::Verify;
    if (str == "general")  return AgentType::GeneralPurpose;
    if (str == "custom")   return AgentType::Custom;
    return std::nullopt;
}

// Agent Configuration
// =========================================================================

struct AgentConfig {
    int max_turns = 200;           // Max agentic loop iterations
    int max_depth = 3;             // Max recursive agent nesting depth
    std::string default_model = "claude-sonnet-4-20250514";
    std::vector<std::string> allowed_tools;  // Empty = inherit all from parent
    std::vector<std::string> denied_tools;   // Explicitly blocked tools for sub-agents
    std::optional<std::string> parent_agent_id;
    std::optional<std::string> parent_permission_mode;
    bool prefer_in_process_teammate = false;
};

struct AgentLivePermissionCheck {
    bool allowed = true;
    std::optional<std::string> updated_input_json;
    std::optional<std::string> message;
};

using AgentLivePermissionCheckFn = std::function<AgentLivePermissionCheck(
    std::string_view tool_name,
    std::string_view input_json,
    std::string_view tool_use_id
)>;

} // namespace loom::tools

// Re-export the DTOs under the agent namespace spelling so importers that
// name them loom::tools::agent::X can depend on this zero-service leaf alone.
export namespace loom::tools::agent {

using loom::tools::AgentConfig;
using loom::tools::AgentLivePermissionCheck;
using loom::tools::AgentLivePermissionCheckFn;

} // namespace loom::tools::agent
