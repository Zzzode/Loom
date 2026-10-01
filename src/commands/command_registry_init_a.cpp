/// @file command_registry_init_a.cpp
/// @brief Group A registration: core commands (commit, review, config, help, etc.)
module loom.commands.registry;

import loom.commands.commit;
import loom.commands.review;
import loom.commands.config;
import loom.commands.context;
import loom.commands.diff;
import loom.commands.mcp_cmd;
import loom.commands.compact;
import loom.commands.help;
import loom.commands.doctor;
import loom.commands.clear;
import loom.commands.add_dir;
import loom.commands.agents;
import loom.commands.btw;
import loom.commands.advisor;
import loom.commands.bridge_kick;
import loom.commands.brief;
import loom.commands.color;
import loom.commands.ctx_viz;

namespace loom::commands {

void register_group_a_commands(CommandRegistry& registry) {
    registry.register_command<CommitCommand>();
    registry.register_command<ReviewCommand>();
    registry.register_command<ConfigCommand>();
    registry.register_command<McpCommand>();
    registry.register_command<CompactCommand>();
    registry.register_command<HelpCommand>();
    registry.register_command<DoctorCommand>();
    registry.register_command<ClearCommand>();
    registry.register_command<AddDirCommand>();
    registry.register_command<AgentsCommand>();
    registry.register_command<BtwCommand>();
    registry.register_command<AdvisorCommand>();
    registry.register_command<BridgeKickCommand>();
    registry.register_command<BriefCommand>();
    registry.register_command<ColorCommand>();
    registry.register_command<CtxVizCommand>();
    registry.register_command<ContextCommand>();
    registry.register_command<DiffCommand>();
}

} // namespace loom::commands