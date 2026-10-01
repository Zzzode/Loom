/// @file ultraplan.cppm
/// @brief UltraplanCommand implementing the /ultraplan slash command.
/// Ultraplan command for expanded planning flows.
module;


export module loom.commands.ultraplan;

import std;

import loom.types.types;
import loom.commands.command;

export namespace loom::commands {

using namespace loom::core;

/// UltraplanCommand implements the /ultraplan slash command.
/// Ultraplan command for expanded planning flows.
class UltraplanCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "ultraplan",
            .description = "Ultraplan command",
            .args = {},
            .category = "planning",
            .aliases = {},
            .hidden = false,
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext&) {
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext&) {
        return CommandResult::success(
            "The /ultraplan command is not enabled in this build. "
            "It spawns a remote Loom-on-the-Web plan session, which is gated behind an external flag. "
            "For local planning, use /plan to enter plan mode instead.");
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view) {
        return {};
    }
};

} // namespace loom::commands
