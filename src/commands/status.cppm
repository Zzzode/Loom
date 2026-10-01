/// @file status.cppm
/// @brief StatusCommand implementing the /status slash command.
/// Shows Loom status.
module;


export module loom.commands.status;

import std;

import loom.types.types;
import loom.commands.command;

export namespace loom::commands {

using namespace loom::core;

/// StatusCommand implements the /status slash command.
/// Shows Loom status.
class StatusCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "status",
            .description = "Show Loom status including model, account, API connectivity, and tool statuses",
            .args = {},
            .category = "info",
            .aliases = {},
            .hidden = false,
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext&) {
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext&) {
        return CommandResult::success(
            std::string("Loom Status:\n") +
            "Model: Not configured\n" +
            "Status: Offline");
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view) {
        return {};
    }
};

} // namespace loom::commands
