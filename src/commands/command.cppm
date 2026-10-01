/// @file command.cppm
/// @brief Slash command module for handling CLI commands like /commit, /review, /clear,
/// /config, /help, etc. Provides command registry, parsing, and execution.
module;

#include <cstdint>

export module loom.commands.command;

import std;

import loom.types.types;

export namespace loom::core {

// ============================================================
// Command Context
// ============================================================

/// Slash command argument type
enum class ArgType : std::uint8_t {
    None,
    Text,
    String = Text,
    FilePath,
    Number,
    Boolean,
    Choice,
};

/// Slash command argument definition
struct CommandArg {
    std::string name;
    std::string description;
    ArgType type{ArgType::Text};
    bool required{false};
    std::vector<std::string> choices{};
    std::optional<std::string> default_value{};
};

/// Registered command metadata used by help, completion and dispatch.
struct CommandDefinition {
    std::string name;
    std::string description;
    std::vector<CommandArg> args{};
    std::string category{"general"};
    std::vector<std::string> aliases{};
    bool hidden{false};
    // SL-03: static argument hint shown inline after the command while the user
    // types args (e.g. "set <key> <value>"). Faithful to TS command.argumentHint.
    // `{}` in-class initializer so existing partial designated initializers in
    // each command's definition() don't trip -Wmissing-designated-field-
    // initializers (the project builds with -Werror).
    std::string argument_hint{};
};

/// Parsed command invocation.
struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
    std::string raw;
};

/// Command execution status.
enum class CommandStatus : std::uint8_t {
    Succeeded,
    Failed,
    Injected,
};

/// Context available to command handlers
using RuntimeMessageProvider = std::vector<Message> (*)(void*);
using RuntimeCompactApplier = VoidResult (*)(void*);

struct CommandContext {
    std::vector<std::string> args;
    std::string raw_input;
    std::string cwd;
    void* runtime_state = nullptr;
    RuntimeMessageProvider compact_message_provider = nullptr;
    RuntimeCompactApplier compact_applier = nullptr;

    // AppState access bridge (set by app.cppm)
    void* app_store = nullptr;  // opaque: loom::state::AppStore*
    using StateDispatchFn = void(*)(void* store, int action_type, const void* payload);
    StateDispatchFn dispatch_fn = nullptr;
    using StateGetFn = const void*(*)(void* store);
    StateGetFn get_state_fn = nullptr;

    // Convenience helpers
    void dispatch_action(int action_type, const void* payload = nullptr) const {
        if (dispatch_fn && app_store) dispatch_fn(app_store, action_type, payload);
    }
    const void* get_app_state() const {
        if (get_state_fn && app_store) return get_state_fn(app_store);
        return nullptr;
    }
};

// ============================================================
// Command Result
// ============================================================

/// Result of command execution
struct CommandResult {
    bool ok{true};
    std::string message;
    std::optional<std::string> metadata{};
    CommandStatus status{CommandStatus::Succeeded};

    [[nodiscard]] static CommandResult success(std::string message) {
        return CommandResult{true, std::move(message), std::nullopt, CommandStatus::Succeeded};
    }

    [[nodiscard]] static CommandResult fail(std::string message) {
        return CommandResult{false, std::move(message), std::nullopt, CommandStatus::Failed};
    }

    [[nodiscard]] static CommandResult inject(std::string prompt) {
        return CommandResult{true, std::move(prompt), std::nullopt, CommandStatus::Injected};
    }

    [[nodiscard]] static CommandResult exit() {
        return CommandResult{true, "Goodbye!", "EXIT", CommandStatus::Succeeded};
    }
};

/// Polymorphic command interface used by the registry.
class ICommand {
public:
    virtual ~ICommand() = default;
    [[nodiscard]] virtual const CommandDefinition& definition() const noexcept = 0;
    [[nodiscard]] virtual VoidResult validate(const CommandContext&) = 0;
    [[nodiscard]] virtual Result<CommandResult> execute(const CommandContext& ctx) = 0;
    [[nodiscard]] virtual std::vector<std::string> complete(std::string_view partial) { (void)partial; return {}; }
};

template <typename Command>
class CommandModel final : public ICommand {
    Command command_{};
    CommandDefinition definition_{Command::definition()};

public:
    [[nodiscard]] const CommandDefinition& definition() const noexcept override { return definition_; }
    [[nodiscard]] VoidResult validate(const CommandContext& ctx) override { return command_.validate(ctx); }
    [[nodiscard]] Result<CommandResult> execute(const CommandContext& ctx) override { return command_.execute(ctx); }
    [[nodiscard]] std::vector<std::string> complete(std::string_view partial) override { return command_.complete(partial); }
};

// ============================================================
// Command Registry
// ============================================================

/// Registry for slash commands
class CommandRegistry {
    std::unordered_map<std::string, std::unique_ptr<ICommand>> commands_;
    std::unordered_map<std::string, std::string> alias_map_;

public:
    CommandRegistry() = default;

    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) noexcept = default;
    CommandRegistry& operator=(CommandRegistry&&) noexcept = default;

    /// Register a command
    template <typename Command>
    void register_command() {
        auto command = std::make_unique<CommandModel<Command>>();
        auto name = command->definition().name;
        for (const auto& alias : command->definition().aliases) {
            alias_map_[alias] = name;
        }
        commands_[name] = std::move(command);
    }

    /// Check if a command exists
    [[nodiscard]] bool has_command(const std::string& name) const {
        return contains(name);
    }

    [[nodiscard]] bool contains(std::string_view name) const {
        auto key = std::string(name);
        if (commands_.contains(key)) return true;
        auto alias = alias_map_.find(key);
        return alias != alias_map_.end() && commands_.contains(alias->second);
    }

    [[nodiscard]] ICommand* get(std::string_view name) const {
        auto key = std::string(name);
        auto it = commands_.find(key);
        if (it != commands_.end()) return it->second.get();

        auto alias_it = alias_map_.find(key);
        if (alias_it != alias_map_.end()) {
            auto target = commands_.find(alias_it->second);
            if (target != commands_.end()) return target->second.get();
        }

        return nullptr;
    }

    [[nodiscard]] static std::optional<ParsedCommand> parse(std::string_view input) {
        if (input.empty() || input.front() != '/') return std::nullopt;
        ParsedCommand parsed;
        parsed.raw = std::string(input);
        input.remove_prefix(1);
        std::istringstream iss{std::string(input)};
        iss >> parsed.name;
        std::string arg;
        while (iss >> arg) parsed.args.push_back(arg);
        if (parsed.name.empty()) return std::nullopt;
        return parsed;
    }

    /// Execute a command from input
    [[nodiscard]] std::optional<CommandResult> execute(const std::string& input,
                                                       CommandContext ctx) const {
        if (input.empty() || input[0] != '/') {
            return std::nullopt;
        }

        std::istringstream iss(input.substr(1));
        std::string cmd_name;
        iss >> cmd_name;

        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }

        if (auto* typed_cmd = get(cmd_name)) {
            ctx.args = std::move(args);
            ctx.raw_input = input;
            if (auto validation = typed_cmd->validate(ctx); !validation) {
                return CommandResult::fail(validation.error().message);
            }
            auto result = typed_cmd->execute(ctx);
            if (!result) {
                return CommandResult::fail(result.error().message);
            }
            return *result;
        }

        return CommandResult{false, std::format("Unknown command: /{}", cmd_name), std::nullopt};
    }

    /// Execute a command from input without runtime context.
    [[nodiscard]] std::optional<CommandResult> execute(const std::string& input) const {
        return execute(input, CommandContext{});
    }

    /// Generate help text
    [[nodiscard]] std::string generate_help() const {
        std::string help = "Available commands:\n\n";
        std::vector<std::string> seen;
        for (const auto& [name, cmd] : commands_) {
            const auto& def = cmd->definition();
            if (def.hidden) continue;
            help += std::format("/{} - {}\n", name, def.description);
            seen.push_back(name);
        }
        return help;
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view partial_input) const {
        std::vector<std::string> result;
        auto prefix = std::string(partial_input);
        if (!prefix.empty() && prefix.front() == '/') prefix.erase(prefix.begin());
        for (const auto& [name, _] : commands_) {
            if (name.starts_with(prefix)) result.push_back('/' + name);
        }
        return result;
    }

    [[nodiscard]] std::vector<const CommandDefinition*> visible_commands() const {
        std::vector<const CommandDefinition*> result;
        for (const auto& [_, cmd] : commands_) {
            if (!cmd->definition().hidden) result.push_back(&cmd->definition());
        }
        return result;
    }

    /// SL-01: return a hidden command whose name exactly matches @a name, or
    /// nullptr. TS lets users reach hidden commands by typing their full name
    /// (commandSuggestions.ts:391-401 hiddenExact); cpp visible_commands()
    /// otherwise hides them entirely, so this is the escape hatch.
    [[nodiscard]] const CommandDefinition* hidden_command_if_exact(
        std::string_view name) const {
        for (const auto& [n, cmd] : commands_) {
            if (cmd->definition().hidden && n == name) return &cmd->definition();
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> command_names() const {
        std::vector<std::string> names;
        names.reserve(commands_.size());
        for (const auto& [name, cmd] : commands_) {
            if (!cmd->definition().hidden) names.push_back(name);
        }
        std::ranges::sort(names);
        names.erase(std::ranges::unique(names).begin(), names.end());
        return names;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return commands_.size();
    }
};

} // namespace loom::core
