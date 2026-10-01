/// @file command_registry_init_d.cpp
/// @brief Group D registration: system commands (permissions, plugin, etc.)
module loom.commands.registry;

import loom.commands.permissions_cmd;
import loom.commands.plugin_cmd;
import loom.commands.usage;
import loom.commands.branch;
import loom.commands.chrome;
import loom.commands.copy_cmd;
import loom.commands.desktop;
import loom.commands.export_cmd;
import loom.commands.good_loom;
import loom.commands.mobile;
import loom.commands.stickers;
import loom.commands.tasks_cmd;
import loom.commands.skills_cmd;
import loom.commands.keybindings_cmd;

namespace cc::commands {

void register_group_d_commands(CommandRegistry& registry) {
    registry.register_command<PermissionsCommand>();
    registry.register_command<PluginCommand>();
    registry.register_command<UsageCommand>();
    registry.register_command<BranchCommand>();
    registry.register_command<ChromeCommand>();
    registry.register_command<CopyCommand>();
    registry.register_command<DesktopCommand>();
    registry.register_command<ExportCommand>();
    registry.register_command<GoodLoomCommand>();
    registry.register_command<KeybindingsCommand>();
    registry.register_command<MobileCommand>();
    registry.register_command<StickersCommand>();
    registry.register_command<TasksCommand>();
    registry.register_command<SkillsCommand>();
}

} // namespace cc::commands