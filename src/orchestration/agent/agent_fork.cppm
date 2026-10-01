module;

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>

export module loom.orchestration.agent.fork;

import std;

import loom.utils.error;
import loom.tools.tool;
import loom.serdes.json;
import loom.tools.agent_runtime;
import loom.tools.agent_constants;
import loom.tools.web_fetch;
import loom.services.api.client;
import loom.services.api.streaming;
import loom.orchestration.agent.utils;

export namespace loom::tools::agent::fork_ {

namespace fs = std::filesystem;

using loom::core::Tool;
using loom::core::ToolInput;
using loom::core::ToolResult;
using loom::core::ToolDefinition;
using loom::services::api::Message;
using loom::services::api::ContentBlock;
using loom::services::api::ContentBlockType;
using loom::tools::agent::utils::AgentExecutionPlan;
using loom::tools::agent::utils::message_from_json_value;
using loom::tools::agent::utils::text_contains_fork_boilerplate;
using loom::tools::agent::utils::message_json_object;
using loom::tools::agent::utils::agent_tool_input_omits_agent_type;

[[nodiscard]] inline std::vector<Message> forked_messages_from_parent_assistant(
    std::string_view directive,
    Message assistant_message
) {
    assistant_message.role = "assistant";

    std::vector<ContentBlock> tool_uses;
    for (const auto& block : assistant_message.content) {
        if (block.type == ContentBlockType::ToolUse && !block.tool_use_id.empty()) {
            tool_uses.push_back(block);
        }
    }

    const auto directive_message = loom::tools::agent_runtime::build_fork_child_message(directive);
    if (tool_uses.empty()) {
        return {Message::from_text("user", directive_message)};
    }

    Message missing_tool_results;
    missing_tool_results.role = "user";
    for (const auto& tool_use : tool_uses) {
        missing_tool_results.content.push_back(ContentBlock{
            .type = ContentBlockType::ToolResult,
            .text = "Fork started \u2014 processing in background",
            .tool_use_id = tool_use.tool_use_id,
        });
    }
    missing_tool_results.content.push_back(ContentBlock{
        .type = ContentBlockType::Text,
        .text = directive_message,
    });

    return {std::move(assistant_message), std::move(missing_tool_results)};
}

[[nodiscard]] inline std::vector<Message> forked_messages_from_parent_assistant_entries(
    std::string_view directive,
    const std::vector<std::string>& entries
) {
    for (const auto& entry : entries) {
        auto parsed = loom::utils::json::parse(entry);
        if (!parsed) continue;
        if (auto message = message_from_json_value(parsed->root())) {
            return forked_messages_from_parent_assistant(directive, std::move(*message));
        }
    }
    return {};
}

[[nodiscard]] inline bool message_contains_fork_boilerplate(const Message& message) {
    return std::ranges::any_of(message.content, [](const ContentBlock& block) {
        return block.type == ContentBlockType::Text && text_contains_fork_boilerplate(block.text);
    });
}

[[nodiscard]] inline bool messages_contain_fork_boilerplate(const std::vector<Message>& messages) {
    return std::ranges::any_of(messages, [](const Message& message) {
        return message_contains_fork_boilerplate(message);
    });
}

[[nodiscard]] inline bool should_reject_fork_child_agent_call(
    const AgentExecutionPlan& plan,
    std::string_view tool_name,
    std::string_view tool_input_json
) {
    if (!plan.fork_child_context) return false;
    if (tool_name != "Agent") return false;
    return agent_tool_input_omits_agent_type(tool_input_json);
}

[[nodiscard]] inline bool exact_tools_allow_tool(
    const AgentExecutionPlan& plan,
    std::string_view tool_name
) {
    if (!plan.use_exact_tools) return true;
    if (plan.exact_tools.empty()) return true;
    return std::ranges::contains(plan.exact_tools, tool_name);
}

[[nodiscard]] inline bool implicit_fork_injection_replaces_key(std::string_view key) {
    constexpr std::array<std::string_view, 16> keys{
        "query_source",
        "querySource",
        "fork_child",
        "forkChild",
        "run_in_background",
        "parent_system_prompt",
        "parentSystemPrompt",
        "exact_tools",
        "exactTools",
        "available_tools",
        "availableTools",
        "use_exact_tools",
        "useExactTools",
        "parent_assistant_message",
        "parentAssistantMessage",
        "assistantMessage",
    };
    return std::ranges::contains(keys, key);
}

[[nodiscard]] inline std::vector<std::string> exact_tool_names_from_api_tools(
    const std::vector<loom::services::api::ToolDefinition>& tools
) {
    std::vector<std::string> names;
    names.reserve(tools.size());
    for (const auto& tool : tools) {
        if (tool.name.empty() || std::ranges::contains(names, tool.name)) continue;
        names.push_back(tool.name);
    }
    return names;
}

[[nodiscard]] inline std::string build_implicit_fork_agent_input_json(
    std::string_view raw_json,
    const AgentExecutionPlan& parent_plan,
    const Message& parent_assistant_message,
    const std::vector<loom::services::api::ToolDefinition>& parent_tools
) {
    auto parsed = loom::utils::json::parse(raw_json);
    if (!parsed || !parsed->root().is_obj()) return std::string(raw_json);

    loom::utils::json::JsonMutDoc doc;
    auto root = doc.object();
    parsed->root().iter_obj([&](loom::utils::json::JsonVal key, loom::utils::json::JsonVal value) {
        if (!key.is_str()) return;
        auto key_text = key.as_str();
        if (implicit_fork_injection_replaces_key(key_text)) return;
        root.add(key_text, doc.copy_val(value));
    });

    root.add("querySource", doc.string("agent:builtin:fork"));
    root.add("forkChild", doc.boolean(true));
    root.add("run_in_background", doc.boolean(true));
    if (!parent_plan.system_prompt.empty()) {
        root.add("parentSystemPrompt", doc.string(parent_plan.system_prompt));
    }

    auto exact_tools = doc.array();
    for (const auto& name : exact_tool_names_from_api_tools(parent_tools)) {
        exact_tools.append(doc.string(name));
    }
    root.add("exactTools", exact_tools);
    root.add("useExactTools", doc.boolean(true));

    if (auto parent_message = doc.raw_json(message_json_object(parent_assistant_message));
        parent_message.valid()) {
        root.add("parentAssistantMessage", parent_message);
    }

    doc.set_root(root);
    return doc.to_string();
}

} // namespace loom::tools::agent::fork_
