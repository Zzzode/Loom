/// @file config.cppm
/// @brief ConfigCommand implementing the /config slash command.
/// Lists, gets, sets settings; opens config file in editor;
/// shows config file locations; validates config values.
module;

#include <cstdint>

export module loom.commands.config;

import std;

import loom.types.types;
import loom.commands.command;
import loom.config.config;

export namespace loom::commands {

using namespace loom::core;

/// Subcommand for /config
enum class ConfigAction : std::uint8_t {
    List,       // List all settings
    Get,        // Get a specific setting
    Set,        // Set a specific setting
    Open,       // Open config file in editor
    Path,       // Show config file locations
};

/// Known configuration keys with their metadata
struct ConfigKeyInfo {
    std::string_view key;
    std::string_view description;
    std::string_view type;          // "string", "bool", "int", "enum"
    std::optional<std::string_view> default_value;
};

/// ConfigCommand implements the /config slash command.
/// Provides get/set/list/open/path subcommands for managing settings.
class ConfigCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "config",
            .description = "View and modify CLI configuration settings",
            .args = {
                CommandArg{.name = "action", .description = "Subcommand: list, get, set, open, path",
                           .type = ArgType::Choice, .required = false,
                           .choices = {"list", "get", "set", "open", "path"}},
                CommandArg{.name = "key", .description = "Configuration key (for get/set)",
                           .type = ArgType::Text, .required = false},
                CommandArg{.name = "value", .description = "New value (for set)",
                           .type = ArgType::Text, .required = false},
            },
            .category = "session",
            .aliases = {"cfg"},
            .hidden = false,
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext& ctx) {
        if (ctx.args.empty()) return {};  // Default to 'list'

        auto action = parse_action(ctx.args[0]);
        if (!action) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest,
                std::format("Unknown config action: '{}'. Use: list, get, set, open, path",
                           ctx.args[0])
            ));
        }

        // 'get' and 'set' require a key
        if (*action == ConfigAction::Get && ctx.args.size() < 2) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest, "/config get requires a key name"
            ));
        }
        if (*action == ConfigAction::Set && ctx.args.size() < 3) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidRequest, "/config set requires a key and value"
            ));
        }
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext& ctx) {
        // Bare /config or /config open → trigger ConfigDialog via metadata tag
        if (ctx.args.empty() || ctx.args[0] == "open" || ctx.args[0] == "edit") {
            return CommandResult{true, "Opening settings...", "UI:config", CommandStatus::Succeeded};
        }

        auto action = parse_action(ctx.args[0]).value_or(ConfigAction::List);

        // list/get/set read or mutate the EFFECTIVE on-disk settings, so the
        // config must be loaded once before touching them. Without this, the
        // default-constructed manager renders defaults for list/get and a set
        // rewrites the project file from defaults, destroying the file's
        // existing sections and mcpServers entries (RFC-0001 B followup c8).
        // Bare /config, open/edit (handled above), and path need no load.
        if (action == ConfigAction::List || action == ConfigAction::Get ||
            action == ConfigAction::Set) {
            if (auto loaded = ensure_loaded(); !loaded) {
                return CommandResult::fail(loaded.error().message);
            }
        }

        switch (action) {
            case ConfigAction::List: return execute_list();
            case ConfigAction::Get:  return execute_get(ctx.args[1]);
            case ConfigAction::Set:  return execute_set(ctx.args[1], ctx.args[2]);
            case ConfigAction::Open: return execute_open();
            case ConfigAction::Path: return execute_path();
        }
        return CommandResult::fail("Unknown config action");
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view partial) {
        // First argument: subcommands
        static constexpr std::array actions = {"list", "get", "set", "open", "path"};
        std::vector<std::string> suggestions;

        for (auto act : actions) {
            if (std::string_view(act).starts_with(partial)) {
                suggestions.emplace_back(act);
            }
        }

        // If partial looks like a config key, suggest known keys
        if (partial.contains('.')) {
            for (const auto& info : known_keys()) {
                if (info.key.starts_with(partial)) {
                    suggestions.emplace_back(info.key);
                }
            }
        }
        return suggestions;
    }

private:
    ConfigManager config_manager_;
    bool loaded_ = false;  // Ensures load() runs at most once successfully.

    /// Load the existing config tiers before a read/mutation. c23: the latch
    /// is invalidated by an external edit — tier_files_changed() stats the
    /// four tier paths (content hash + size/inode) and forces a reload when
    /// any differs from the snapshot recorded at the last load()/save().
    /// This picks up externally edited config files instead of serving a
    /// stale session snapshot (which a set would then clobber on save),
    /// while preserving the c19 same-session invariant: our own save()
    /// refreshes the saved path's signature, so a get after a set does NOT
    /// reload (a reload would re-apply the LOOM_MODEL env overlay over the
    /// in-memory explicit value). ConfigManager::load() tolerates MISSING
    /// tier files (they surface ConfigNotFound and are skipped); hard parse
    /// errors in the project file return an error, while user/local
    /// parse errors are soft (C6 §A). The guard latches only on success:
    /// after a hard failure the caller sees the error and a later invocation
    /// retries once the file is repaired, rather than being permanently
    /// stuck on default settings.
    [[nodiscard]] VoidResult ensure_loaded() {
        if (loaded_ && !config_manager_.tier_files_changed()) return {};
        auto result = config_manager_.load();
        if (!result) return std::unexpected(result.error());
        loaded_ = true;
        return {};
    }

    /// Parse action string to enum
    [[nodiscard]] static std::optional<ConfigAction> parse_action(std::string_view str) {
        if (str == "list" || str == "ls")  return ConfigAction::List;
        if (str == "get")                   return ConfigAction::Get;
        if (str == "set")                   return ConfigAction::Set;
        if (str == "open" || str == "edit") return ConfigAction::Open;
        if (str == "path" || str == "paths") return ConfigAction::Path;
        return std::nullopt;
    }

    /// List all current configuration settings
    [[nodiscard]] Result<CommandResult> execute_list() {
        const auto& settings = config_manager_.settings();
        std::string output = "Current Configuration:\n\n";

        output += std::format("  model             = {}\n", settings.model.default_model);
        output += std::format("  maxOutputTokens   = {}\n", settings.model.max_output_tokens);
        output += std::format("  extendedThinking  = {}\n",
                             settings.model.extended_thinking ? "true" : "false");
        output += std::format("  contextWindowSize = {}\n", settings.model.context_window_size);
        output += "\n";
        output += std::format("  showThinking      = {}\n",
                             settings.display.show_thinking ? "true" : "false");
        output += std::format("  showTokenUsage    = {}\n",
                             settings.display.show_token_usage ? "true" : "false");
        output += std::format("  theme             = {}\n", settings.display.theme);
        output += "\n";
        output += std::format("  timeoutSeconds    = {}s\n", settings.network.timeout_seconds);
        output += std::format("  maxRetries        = {}\n", settings.network.max_retries);
        output += std::format("  verifySsl         = {}\n",
                             settings.network.verify_ssl ? "true" : "false");
        output += std::format("  apiKey            = {}\n",
                             settings.network.api_key ? "***" : "(not set)");
        output += "\n";
        output += std::format("  permissions.allow_bash  = {}\n",
                             settings.permissions.allow_bash ? "true" : "false");
        output += std::format("  permissions.allow_write = {}\n",
                             settings.permissions.allow_file_write ? "true" : "false");

        return CommandResult::success(std::move(output));
    }

    /// Get a specific configuration value
    [[nodiscard]] Result<CommandResult> execute_get(std::string_view key) {
        auto value = resolve_key(key);
        if (!value) {
            return CommandResult::fail(std::format("Unknown config key: '{}'", key));
        }
        return CommandResult::success(std::format("{} = {}", key, *value));
    }

    /// Set a specific configuration value
    [[nodiscard]] Result<CommandResult> execute_set(std::string_view key, std::string_view value) {
        // Validate the value before applying
        if (auto result = validate_value(key, value); !result) {
            return std::unexpected(result.error());
        }

        auto apply_result = apply_setting(key, value);
        if (!apply_result) return std::unexpected(apply_result.error());

        // c19: this is an EXPLICIT user write, so mark the leaf as user intent
        // before the full save. Without this, `/config set model X`
        // with LOOM_MODEL still exported would only persist X when X differs
        // from the env value; the marker makes user intent win even when the
        // user sets the value back to the env value. The (section, leaf) pair
        // comes from the shared spec table, so it cannot drift from the
        // serializer's keys (a non-model key is a no-op inside the marker).
        if (const auto* spec = loom::core::ConfigManager::find_user_setting(key)) {
            config_manager_.clear_env_provenance(spec->section, spec->leaf);
        }

        // Persist changes.
        //
        // c21: save() patches the KNOWN sections leaf-by-leaf into the parsed
        // project file, so unknown top-level keys (e.g. "x-custom") and
        // unknown keys inside a known section (e.g. a custom "model" leaf)
        // survive a /config set. All KNOWN sections (model/display/network/…)
        // are the loaded values, and C6's §B filter — gated on the completed
        // load() above — emits the project file's OWN mcpServers entries with
        // their own values while omitting user/local-only entries that may
        // carry Authorization headers.
        if (auto save_result = config_manager_.save(); !save_result) {
            return std::unexpected(save_result.error());
        }

        return CommandResult::success(std::format("Set {} = {}", key, value));
    }

    /// Open config file in the user's editor
    [[nodiscard]] Result<CommandResult> execute_open() {
        auto path = config_manager_.project_config_path();
        // Return path for the shell to open with $EDITOR
        return CommandResult::success(
            std::format("Opening config file: {}\nRun: $EDITOR {}", path.string(), path.string())
        );
    }

    /// Show config file locations
    [[nodiscard]] Result<CommandResult> execute_path() {
        auto project = config_manager_.project_config_path();

        std::string output = "Configuration file locations:\n\n";
        output += std::format("  Project: {} {}\n", project.string(),
                             std::filesystem::exists(project) ? "(exists)" : "(not found)");
        output += "\nPriority: CLI flags > env vars > project config > defaults\n";

        return CommandResult::success(std::move(output));
    }

    /// Resolve a flat config key to its current value
    [[nodiscard]] std::optional<std::string> resolve_key(std::string_view key) const {
        const auto& s = config_manager_.settings();
        if (key == "model")             return s.model.default_model;
        if (key == "maxOutputTokens")   return std::to_string(s.model.max_output_tokens);
        if (key == "extendedThinking")  return s.model.extended_thinking ? "true" : "false";
        if (key == "showThinking")      return s.display.show_thinking ? "true" : "false";
        if (key == "theme")             return s.display.theme;
        if (key == "timeoutSeconds")    return std::to_string(s.network.timeout_seconds);
        if (key == "maxRetries")        return std::to_string(s.network.max_retries);
        if (key == "permissions.allow_bash") return s.permissions.allow_bash ? "true" : "false";
        return std::nullopt;
    }

    /// Validate a value before applying it
    [[nodiscard]] static VoidResult validate_value(std::string_view key, std::string_view value) {
        // Boolean keys
        if (key == "extendedThinking" || key == "showThinking" ||
            key == "showTokenUsage" || key == "verifySsl" ||
            key.starts_with("permissions.")) {
            if (value != "true" && value != "false") {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidRequest,
                    std::format("'{}' must be 'true' or 'false', got: '{}'", key, value)
                ));
            }
        }
        // Integer keys
        if (key == "maxOutputTokens" || key == "timeoutSeconds" ||
            key == "maxRetries") {
            try { (void)std::stoul(std::string(value)); }
            catch (...) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidRequest,
                    std::format("'{}' must be a positive integer, got: '{}'", key, value)
                ));
            }
        }
        return {};
    }

    /// Apply a validated setting to the config manager
    [[nodiscard]] VoidResult apply_setting(std::string_view key, std::string_view value) {
        auto& s = config_manager_.settings_mut();
        if (key == "model")               { s.model.default_model = value; return {}; }
        if (key == "maxOutputTokens")     { s.model.max_output_tokens = std::stoul(std::string(value)); return {}; }
        if (key == "extendedThinking")    { s.model.extended_thinking = (value == "true"); return {}; }
        if (key == "showThinking")        { s.display.show_thinking = (value == "true"); return {}; }
        if (key == "theme")               { s.display.theme = value; return {}; }
        if (key == "timeoutSeconds")      { s.network.timeout_seconds = std::stoul(std::string(value)); return {}; }
        if (key == "maxRetries")          { s.network.max_retries = std::stoul(std::string(value)); return {}; }
        if (key == "permissions.allow_bash") { s.permissions.allow_bash = (value == "true"); return {}; }

        return std::unexpected(Error::make(ErrorCode::ConfigNotFound,
            std::format("Unknown or read-only key: '{}'", key)));
    }

    /// Get all known configuration keys with metadata
    [[nodiscard]] static std::vector<ConfigKeyInfo> known_keys() {
        return {
            {"model",             "LLM model to use",             "string", ""},
            {"maxOutputTokens",   "Maximum output token count",   "int",    "16384"},
            {"extendedThinking",  "Enable extended thinking",     "bool",   "false"},
            {"showThinking",      "Show thinking blocks",         "bool",   "true"},
            {"theme",             "Color theme (auto/dark/light)","enum",   "auto"},
            {"timeoutSeconds",    "Request timeout (seconds)",    "int",    "120"},
            {"maxRetries",        "Max retry attempts",           "int",    "3"},
            {"permissions.allow_bash", "Allow bash execution",    "bool",   "true"},
        };
    }
};

} // namespace loom::commands
