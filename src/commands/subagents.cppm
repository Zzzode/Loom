/// Additive interactive runtime-instance entry; /agents keeps definitions.
export module loom.commands.subagents;
import std;
import loom.types.types;
import loom.commands.command;
export namespace loom::commands {
using namespace loom::core;
class SubagentsCommand {
public:
    [[nodiscard]] static CommandDefinition definition();
    [[nodiscard]] std::vector<std::string> complete(std::string_view partial);
    [[nodiscard]] loom::core::VoidResult validate(const CommandContext& ctx);
    [[nodiscard]] loom::core::Result<CommandResult> execute(const CommandContext& ctx);
};
}
