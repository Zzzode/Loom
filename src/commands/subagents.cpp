module loom.commands.subagents;
import std;

namespace loom::commands {
CommandDefinition SubagentsCommand::definition() {
    return CommandDefinition{
        .name = "subagents",
        .description = "Browse runtime instances, history and controls",
        .category = "agents",
        .argument_hint = "[history|team|<agent-id>]",
    };
}

std::vector<std::string> SubagentsCommand::complete(std::string_view partial) {
    std::vector<std::string> result;
    for (const auto* candidate : {"history", "team"})
        if (std::string_view(candidate).starts_with(partial)) result.emplace_back(candidate);
    return result;
}

loom::core::VoidResult SubagentsCommand::validate(const CommandContext&) {
    return {};
}

loom::core::Result<CommandResult> SubagentsCommand::execute(const CommandContext&) {
    return CommandResult::success(
        "Open /subagents in the interactive terminal to inspect runtime instances.");
}
}  // namespace loom::commands
