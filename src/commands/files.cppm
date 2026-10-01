/// @file files.cppm
/// @brief FilesCommand implementing the /files slash command.
/// Shows all files currently in context.
module;


export module loom.commands.files;

import std;

import loom.types.types;
import loom.commands.command;

export namespace loom::commands {

using namespace loom::core;

/// FilesCommand implements the /files slash command.
/// Shows all files currently in context.
class FilesCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "files",
            .description = "Show all files currently in context",
            .args = {},
            .category = "context",
            .aliases = {},
            .hidden = false,
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext&) {
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext&) {
        return CommandResult::fail("Context file provider is not configured.");
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view) {
        return {};
    }
};

} // namespace loom::commands
