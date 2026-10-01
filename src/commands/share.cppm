/// @file share.cppm
/// @brief ShareCommand implementing the /share slash command.
/// Shares the current session.
module;


export module loom.commands.share;

import std;

import loom.types.types;
import loom.commands.command;

export namespace loom::commands {

using namespace loom::core;

/// ShareCommand implements the /share slash command.
/// Shares the current session.
class ShareCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "share",
            .description = "Share current session",
            .args = {},
            .category = "session",
            .aliases = {},
            .hidden = false,
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext&) {
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext&) {
        return CommandResult::success(
            "The /share command is not available in this native build. "
            "To share a session, export the transcript from session storage "
            "(<data-dir>/projects/<project>/<session-id>/messages.jsonl) and publish externally.");
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view) {
        return {};
    }
};

} // namespace loom::commands
