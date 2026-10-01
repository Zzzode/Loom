/// @file command_registry_init_b.cpp
/// @brief Group B registration: utility commands (effort, fast, files, stats, etc.)
module loom.commands.registry;

import loom.commands.effort;
import loom.commands.env;
import loom.commands.fast;
import loom.commands.feedback;
import loom.commands.files;
import loom.commands.heapdump;
import loom.commands.hooks;
import loom.commands.ide;
import loom.commands.issue;
import loom.commands.memory;
import loom.commands.passes;
import loom.commands.rename;
import loom.commands.rewind;
import loom.commands.share;
import loom.commands.stats;
import loom.commands.status;
import loom.commands.summary;
import loom.commands.tag;

namespace cc::commands {

void register_group_b_commands(CommandRegistry& registry) {
    registry.register_command<EffortCommand>();
    registry.register_command<EnvCommand>();
    registry.register_command<FastCommand>();
    registry.register_command<FeedbackCommand>();
    registry.register_command<FilesCommand>();
    registry.register_command<HeapdumpCommand>();
    registry.register_command<HooksCommand>();
    registry.register_command<IdeCommand>();
    registry.register_command<IssueCommand>();
    registry.register_command<MemoryCommand>();
    registry.register_command<PassesCommand>();
    registry.register_command<RenameCommand>();
    registry.register_command<RewindCommand>();
    registry.register_command<ShareCommand>();
    registry.register_command<StatsCommand>();
    registry.register_command<StatusCommand>();
    registry.register_command<SummaryCommand>();
    registry.register_command<TagCommand>();
}

} // namespace cc::commands