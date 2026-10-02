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

/// Known configuration keys with their metadata. Projected from
/// ConfigManager::user_setting_specs() — the spec table is the single
/// source of truth for the key set; this struct only carries the
/// presentation shape used by the completion list.
struct ConfigKeyInfo {
    std::string_view key;
    std::string_view description;
    std::string_view type;          // "string", "bool", "int", "enum"
    std::optional<std::string> default_value;
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

    /// Map a UserSettingKind to the short type token carried in
    /// ConfigKeyInfo. Number (a floating-point scalar such as temperature)
    /// is reported as "int" because the completion metadata has no
    /// floating-point token; the value is still validated as a number.
    [[nodiscard]] static constexpr std::string_view
    type_name_for(UserSettingKind kind) noexcept {
        switch (kind) {
            case UserSettingKind::String:      return "string";
            case UserSettingKind::Boolean:     return "bool";
            case UserSettingKind::UInteger:    return "int";
            case UserSettingKind::Number:      return "int";
            case UserSettingKind::Enumeration: return "enum";
        }
        return "string";
    }

    /// Project a spec's default value from a default-constructed Settings,
    /// so the Settings member initializers stay the single source of truth
    /// for defaults. Unset optional leaves report nullopt.
    [[nodiscard]] static std::optional<std::string>
    default_value_for(const UserSettingSpec& spec) {
        const Settings d{};
        const std::string_view key = spec.key;
        if (key == "model")              return d.model.default_model;
        if (key == "maxOutputTokens")    return std::to_string(d.model.max_output_tokens);
        if (key == "temperature")        return d.model.temperature
            ? std::optional<std::string>(std::format("{}", *d.model.temperature))
            : std::nullopt;
        if (key == "extendedThinking")   return d.model.extended_thinking ? "true" : "false";
        if (key == "thinkingBudget")     return d.model.thinking_budget
            ? std::optional<std::string>(std::to_string(*d.model.thinking_budget))
            : std::nullopt;
        if (key == "contextWindowSize")  return std::to_string(d.model.context_window_size);
        if (key == "showThinking")       return d.display.show_thinking ? "true" : "false";
        if (key == "showTokenUsage")     return d.display.show_token_usage ? "true" : "false";
        if (key == "compactMode")        return d.display.compact_mode ? "true" : "false";
        if (key == "theme")              return d.display.theme;
        if (key == "lineWidth")          return d.display.line_width
            ? std::optional<std::string>(std::to_string(*d.display.line_width))
            : std::nullopt;
        if (key == "timeoutSeconds")     return std::to_string(d.network.timeout_seconds);
        if (key == "maxRetries")         return std::to_string(d.network.max_retries);
        if (key == "permissions.allow_bash")       return d.permissions.allow_bash ? "true" : "false";
        if (key == "permissions.allow_file_write") return d.permissions.allow_file_write ? "true" : "false";
        if (key == "permissions.allow_network")    return d.permissions.allow_network ? "true" : "false";
        return std::nullopt;
    }

    /// Resolve a flat config key to its current value. The key set is the
    /// closed spec table — find_user_setting rejects unknown keys. The
    /// key-to-field mapping is not in the spec table (it lives in the
    /// Settings struct), so the if-chain stays; it now covers ALL 16
    /// projected keys instead of the previous 8.
    [[nodiscard]] std::optional<std::string> resolve_key(std::string_view key) const {
        if (loom::core::ConfigManager::find_user_setting(key) == nullptr) {
            return std::nullopt;
        }
        const auto& s = config_manager_.settings();
        if (key == "model")             return s.model.default_model;
        if (key == "maxOutputTokens")   return std::to_string(s.model.max_output_tokens);
        if (key == "temperature")       return s.model.temperature
            ? std::optional<std::string>(std::format("{}", *s.model.temperature))
            : std::optional<std::string>("unset");
        if (key == "extendedThinking")  return s.model.extended_thinking ? "true" : "false";
        if (key == "thinkingBudget")    return s.model.thinking_budget
            ? std::optional<std::string>(std::to_string(*s.model.thinking_budget))
            : std::optional<std::string>("unset");
        if (key == "contextWindowSize") return std::to_string(s.model.context_window_size);
        if (key == "showThinking")      return s.display.show_thinking ? "true" : "false";
        if (key == "showTokenUsage")    return s.display.show_token_usage ? "true" : "false";
        if (key == "compactMode")       return s.display.compact_mode ? "true" : "false";
        if (key == "theme")             return s.display.theme;
        if (key == "lineWidth")         return s.display.line_width
            ? std::optional<std::string>(std::to_string(*s.display.line_width))
            : std::optional<std::string>("unset");
        if (key == "timeoutSeconds")    return std::to_string(s.network.timeout_seconds);
        if (key == "maxRetries")        return std::to_string(s.network.max_retries);
        if (key == "permissions.allow_bash")       return s.permissions.allow_bash ? "true" : "false";
        if (key == "permissions.allow_file_write") return s.permissions.allow_file_write ? "true" : "false";
        if (key == "permissions.allow_network")    return s.permissions.allow_network ? "true" : "false";
        return std::nullopt;
    }

    /// Validate a value before applying it. The spec table drives the
    /// type check: Boolean keys accept only "true"/"false", UInteger keys
    /// must parse via stoul, Number keys via stod. Unknown keys (outside
    /// the closed 16-key set, including blocked keys such as verifySsl)
    /// pass through — apply_setting rejects them with the canonical
    /// "Unknown or read-only key" error.
    [[nodiscard]] static VoidResult validate_value(std::string_view key, std::string_view value) {
        const auto* spec = loom::core::ConfigManager::find_user_setting(key);
        if (spec == nullptr) return {};
        switch (spec->kind) {
            case UserSettingKind::Boolean:
                if (value != "true" && value != "false") {
                    return std::unexpected(Error::make(
                        ErrorCode::InvalidRequest,
                        std::format("'{}' must be 'true' or 'false', got: '{}'", key, value)
                    ));
                }
                break;
            case UserSettingKind::UInteger:
                try { (void)std::stoul(std::string(value)); }
                catch (...) {
                    return std::unexpected(Error::make(
                        ErrorCode::InvalidRequest,
                        std::format("'{}' must be a positive integer, got: '{}'", key, value)
                    ));
                }
                break;
            case UserSettingKind::Number:
                try { (void)std::stod(std::string(value)); }
                catch (...) {
                    return std::unexpected(Error::make(
                        ErrorCode::InvalidRequest,
                        std::format("'{}' must be a number, got: '{}'", key, value)
                    ));
                }
                break;
            case UserSettingKind::String:
            case UserSettingKind::Enumeration:
                break;  // No value validation.
        }
        return {};
    }

    /// Apply a validated setting to the config manager. find_user_setting
    /// rejects keys outside the closed 16-key spec set; the if-chain then
    /// maps every projected key to its Settings field. The spec table's
    /// `writable` flag governs the agent-facing settings tool (user-tier
    /// writes) — the /config command writes the project tier and so allows
    /// setting every projected key, including the 9 read-only metadata
    /// keys (theme, showThinking, ...). The key-to-field mapping is not in
    /// the spec table, so the if-chain stays; it now covers ALL 16
    /// projected keys instead of the previous 8.
    [[nodiscard]] VoidResult apply_setting(std::string_view key, std::string_view value) {
        if (loom::core::ConfigManager::find_user_setting(key) == nullptr) {
            return std::unexpected(Error::make(ErrorCode::ConfigNotFound,
                std::format("Unknown or read-only key: '{}'", key)));
        }
        auto& s = config_manager_.settings_mut();
        if (key == "model")               { s.model.default_model = value; return {}; }
        if (key == "maxOutputTokens")     { s.model.max_output_tokens = std::stoul(std::string(value)); return {}; }
        if (key == "temperature")         { s.model.temperature = std::stod(std::string(value)); return {}; }
        if (key == "extendedThinking")    { s.model.extended_thinking = (value == "true"); return {}; }
        if (key == "thinkingBudget")      { s.model.thinking_budget = std::stoul(std::string(value)); return {}; }
        if (key == "contextWindowSize")   { s.model.context_window_size = std::stoul(std::string(value)); return {}; }
        if (key == "showThinking")        { s.display.show_thinking = (value == "true"); return {}; }
        if (key == "showTokenUsage")      { s.display.show_token_usage = (value == "true"); return {}; }
        if (key == "compactMode")         { s.display.compact_mode = (value == "true"); return {}; }
        if (key == "theme")               { s.display.theme = value; return {}; }
        if (key == "lineWidth")           { s.display.line_width = std::stoul(std::string(value)); return {}; }
        if (key == "timeoutSeconds")      { s.network.timeout_seconds = std::stoul(std::string(value)); return {}; }
        if (key == "maxRetries")          { s.network.max_retries = std::stoul(std::string(value)); return {}; }
        if (key == "permissions.allow_bash")       { s.permissions.allow_bash = (value == "true"); return {}; }
        if (key == "permissions.allow_file_write") { s.permissions.allow_file_write = (value == "true"); return {}; }
        if (key == "permissions.allow_network")    { s.permissions.allow_network = (value == "true"); return {}; }
        return std::unexpected(Error::make(ErrorCode::ConfigNotFound,
            std::format("Unknown or read-only key: '{}'", key)));
    }

    /// Get all known configuration keys with metadata, projected from the
    /// ConfigManager spec table so the key set can never drift from the
    /// serializer's keys.
    [[nodiscard]] static std::vector<ConfigKeyInfo> known_keys() {
        std::vector<ConfigKeyInfo> result;
        for (const auto& spec : loom::core::ConfigManager::user_setting_specs()) {
            result.push_back(ConfigKeyInfo{
                .key = spec.key,
                .description = spec.description,
                .type = type_name_for(spec.kind),
                .default_value = default_value_for(spec),
            });
        }
        return result;
    }
};

} // namespace loom::commands
