// Implementation unit for cc.tools.runtime_backends.port — the single
// function-local RuntimeBackendSlots struct and every setter/clearer/
// accessor declared by the unified seam. RFC-0001 B15: one struct of
// std::function slots, one anchor TU, no inline definitions in the module
// interface (inline-def ratchet).
module;

module loom.tools.runtime_backends.port;

import std;

// arch-check: keep-import (all four) — this impl unit uses the imported
// types only UNQUALIFIED inside namespace cc::tools (RuntimeToolExecutor,
// SkillLoaderExecutor, ITool, AgentConfig, ...), which graph_check's textual
// evidence heuristic cannot attribute; the imports are required for the
// std::function slot definitions.
import loom.types.types;              // arch-check: keep-import
import loom.types.tool_types;         // arch-check: keep-import
import loom.tools.tool;               // arch-check: keep-import
import loom.tools.agent_types;        // arch-check: keep-import

namespace cc::tools::detail {

struct RuntimeBackendSlots {
    // B12 SkillLoader discovery (std::nullopt = manual SKILL.md walk runs).
    std::optional<SkillLoaderExecutor> skill_loader;

    // The six lifted runtime tool backends (empty = no handler installed).
    RuntimeToolExecutor lsp;
    RuntimeToolExecutor mcp;
    RuntimeToolExecutor list_mcp_resources;
    RuntimeToolExecutor read_mcp_resource;
    RuntimeToolExecutor mcp_auth;
    RuntimeToolExecutor computer_use;
    // c13b structured config runtime tool.
    RuntimeToolExecutor config;

    // Agent tool factory.
    AgentToolFactory agent_tool_factory;
};

RuntimeBackendSlots& runtime_backend_slots() {
    static RuntimeBackendSlots slots;
    return slots;
}

} // namespace cc::tools::detail

namespace cc::tools {

// ── SkillLoader executor ──────────────────────────────────────────────────
void set_skill_loader_executor(SkillLoaderExecutor executor) {
    detail::runtime_backend_slots().skill_loader = std::move(executor);
}

void clear_skill_loader_executor() {
    detail::runtime_backend_slots().skill_loader.reset();
}

std::optional<SkillLoaderExecutor>& skill_loader_executor_slot() {
    return detail::runtime_backend_slots().skill_loader;
}

// ── The six lifted runtime tool backends ──────────────────────────────────
void set_lsp_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().lsp = std::move(executor);
}
void clear_lsp_backend() { detail::runtime_backend_slots().lsp = nullptr; }
RuntimeToolExecutor& lsp_backend() { return detail::runtime_backend_slots().lsp; }

void set_mcp_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().mcp = std::move(executor);
}
void clear_mcp_backend() { detail::runtime_backend_slots().mcp = nullptr; }
RuntimeToolExecutor& mcp_backend() { return detail::runtime_backend_slots().mcp; }

void set_list_mcp_resources_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().list_mcp_resources = std::move(executor);
}
void clear_list_mcp_resources_backend() {
    detail::runtime_backend_slots().list_mcp_resources = nullptr;
}
RuntimeToolExecutor& list_mcp_resources_backend() {
    return detail::runtime_backend_slots().list_mcp_resources;
}

void set_read_mcp_resource_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().read_mcp_resource = std::move(executor);
}
void clear_read_mcp_resource_backend() {
    detail::runtime_backend_slots().read_mcp_resource = nullptr;
}
RuntimeToolExecutor& read_mcp_resource_backend() {
    return detail::runtime_backend_slots().read_mcp_resource;
}

void set_mcp_auth_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().mcp_auth = std::move(executor);
}
void clear_mcp_auth_backend() { detail::runtime_backend_slots().mcp_auth = nullptr; }
RuntimeToolExecutor& mcp_auth_backend() { return detail::runtime_backend_slots().mcp_auth; }

void set_computer_use_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().computer_use = std::move(executor);
}
void clear_computer_use_backend() {
    detail::runtime_backend_slots().computer_use = nullptr;
}
RuntimeToolExecutor& computer_use_backend() {
    return detail::runtime_backend_slots().computer_use;
}

void set_config_backend(RuntimeToolExecutor executor) {
    detail::runtime_backend_slots().config = std::move(executor);
}
void clear_config_backend() { detail::runtime_backend_slots().config = nullptr; }
RuntimeToolExecutor& config_backend() { return detail::runtime_backend_slots().config; }

// ── Agent tool factory ────────────────────────────────────────────────────
void set_agent_tool_factory(AgentToolFactory factory) {
    detail::runtime_backend_slots().agent_tool_factory = std::move(factory);
}
void clear_agent_tool_factory() {
    detail::runtime_backend_slots().agent_tool_factory = nullptr; }
AgentToolFactory& agent_tool_factory() {
    return detail::runtime_backend_slots().agent_tool_factory;
}

} // namespace cc::tools
