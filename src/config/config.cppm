/// @file config.cppm
/// @brief Configuration module for the Loom CLI.
/// Manages hierarchical settings (global -> project -> CLI flags),
/// environment variable integration, feature flags, and JSON persistence.
module;

#include <cstdlib>
#include <cstdint>
#include <cstdio>

export module cc.config.config;

import std;

import cc.types.types;
import cc.utils.json;
import cc.constants.paths;

export import cc.config.mcp_types;

export namespace cc::core {

// ============================================================
// Feature Flags
// ============================================================

/// Named feature flags that can be toggled at build or runtime
enum class FeatureFlag : std::uint32_t {
    Proactive          = 1 << 0,   // Proactive suggestions
    BridgeMode         = 1 << 1,   // IDE bridge integration
    Daemon             = 1 << 3,   // Background daemon mode
    AgentTriggers      = 1 << 4,   // Automatic agent triggering
    MonitorTool        = 1 << 5,   // System monitoring tool
    Templates          = 1 << 6,   // Template system
    BackgroundSessions = 1 << 7,   // Background session support
    ExtendedThinking   = 1 << 8,   // Extended thinking mode
    MultiAgent         = 1 << 9,   // Multi-agent orchestration
    SkillSystem        = 1 << 10,  // Skill loading system
};

/// Bitwise operations for combining feature flags
[[nodiscard]] constexpr FeatureFlag operator|(FeatureFlag a, FeatureFlag b) noexcept {
    return static_cast<FeatureFlag>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)
    );
}

[[nodiscard]] constexpr FeatureFlag operator&(FeatureFlag a, FeatureFlag b) noexcept {
    return static_cast<FeatureFlag>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)
    );
}

/// Feature flag set with runtime toggle support
class FeatureFlags {
    std::uint32_t flags_ = 0;

public:
    FeatureFlags() = default;
    explicit FeatureFlags(std::uint32_t raw) : flags_(raw) {}

    /// Check if a flag is enabled
    [[nodiscard]] bool is_enabled(FeatureFlag flag) const noexcept {
        return (flags_ & static_cast<std::uint32_t>(flag)) != 0;
    }

    /// Enable a feature flag
    void enable(FeatureFlag flag) noexcept {
        flags_ |= static_cast<std::uint32_t>(flag);
    }

    /// Disable a feature flag
    void disable(FeatureFlag flag) noexcept {
        flags_ &= ~static_cast<std::uint32_t>(flag);
    }

    /// Set flag state explicitly
    void set(FeatureFlag flag, bool enabled) noexcept {
        if (enabled) enable(flag); else disable(flag);
    }

    /// Get raw flag bits
    [[nodiscard]] std::uint32_t raw() const noexcept { return flags_; }
};

// ============================================================
// Settings structure
// ============================================================

/// Model-specific settings
struct ModelSettings {
    std::string default_model = "claude-sonnet-4-20250514";
    std::uint32_t max_output_tokens = 16384;
    std::optional<double> temperature;
    bool extended_thinking = false;
    std::optional<std::uint32_t> thinking_budget;
    std::uint32_t context_window_size = 200000;
};

/// Permission settings for tool execution
struct PermissionSettings {
    bool allow_bash = true;                    // Allow bash command execution
    bool allow_file_write = true;              // Allow file modifications
    bool allow_network = true;                 // Allow network requests
    std::vector<std::string> allowed_paths;    // Whitelisted file paths
    std::vector<std::string> denied_paths;     // Blacklisted file paths
    std::vector<std::string> allowed_commands; // Whitelisted shell commands
    // Raw permissions.deny rules (TS alwaysDenyRules) filtered from the tool
    // list before each API request. Parsed identically by interactive and
    // headless engines so neither path can bypass them.
    std::vector<std::string> deny_rules;
};

/// Display and UI settings
struct DisplaySettings {
    bool show_thinking = true;                 // Display thinking blocks
    bool show_token_usage = false;             // Show token counters
    bool compact_mode = false;                 // Minimal output formatting
    std::optional<std::uint32_t> line_width;   // Terminal line width override
    std::string theme = "auto";               // Color theme (auto, dark, light)
};

/// Network and API connection settings
struct NetworkSettings {
    std::optional<std::string> api_key;            // Anthropic API key
    std::optional<std::string> base_url;           // Custom API endpoint
    std::optional<std::string> proxy;              // HTTP proxy URL
    std::uint32_t timeout_seconds = 120;           // Request timeout
    std::uint32_t max_retries = 3;                 // Retry attempts
    bool verify_ssl = true;                        // SSL certificate verification
};

struct XaaIdpSettings {
    std::string issuer;
    std::string client_id;
    std::optional<int> callback_port;
};

/// Top-level settings aggregating all configuration sections
struct Settings {
    ModelSettings model;
    PermissionSettings permissions;
    DisplaySettings display;
    NetworkSettings network;
    FeatureFlags features;
    std::vector<McpServerConfig> mcp_servers;      // Configured MCP servers
    std::optional<std::string> system_prompt;      // Custom system prompt override
    std::vector<std::string> custom_instructions;  // Additional context instructions
    XaaIdpSettings xaa_idp;                         // XAA IdP configuration
};

// ============================================================
// Configuration source hierarchy
// ============================================================

/// Configuration source priority (lower value = higher priority)
enum class ConfigSource : std::uint8_t {
    CliFlags = 0,      // Command-line arguments (highest priority)
    EnvVars = 1,       // Environment variables
    ProjectConfig = 2, // .loom/config.json in project root
    GlobalConfig = 3,  // ~/.config/loom/config.json
    Defaults = 4,      // Built-in defaults (lowest priority)
};

// ============================================================
// MCP per-tier storage routing (RFC-0001 B followup c6)
// ============================================================

/// Physical MCP config file tiers. Same-name resolution, highest first:
/// Local > Project > User > Global.
enum class McpStorageScope : std::uint8_t {
    Global = 0, // Legacy ~/.config/loom/config.json (read tier; written on remove)
    User   = 1, // $LOOM_CONFIG_DIR/config.json, else ~/.loom/config.json
    Project= 2, // <project>/.loom/config.json (tracked by VCS)
    Local  = 3, // <project>/.loom/config.local.json (gitignored)
};

/// Outcome of a best-effort cross-file MCP server removal (D3). The command
/// renders three cases from {touched, failed}: absent everywhere (both
/// empty), >=1 write succeeded (touched non-empty), present but every write
/// failed (touched empty, failed non-empty).
struct McpRemoveOutcome {
    std::vector<std::filesystem::path> touched;
    std::vector<std::filesystem::path> failed;
};

// ============================================================
// Config Manager
// ============================================================

/// Manages loading, merging, and persisting configuration from multiple sources
class ConfigManager {
    Settings settings_;                     // Resolved effective settings
    std::filesystem::path global_path_;     // Path to global config file
    std::filesystem::path user_path_;       // Path to user config file (empty in 2-arg ctor)
    std::filesystem::path project_path_;    // Path to project config file
    std::filesystem::path local_path_;      // Path to local (gitignored) config file
    bool dirty_ = false;                   // Whether unsaved changes exist

    // RFC-0001 B followup c6 per-tier MCP bookkeeping, repopulated by load().
    // Index order matches McpStorageScope.
    std::array<std::unordered_set<std::string>, 4> mcp_physical_names_;
    std::array<std::vector<McpServerConfig>, 4> mcp_file_servers_;
    std::unordered_map<std::string, McpStorageScope> mcp_owner_;
    // True for a user/local tier whose file exists but is not a parseable
    // JSON object (§A): the tier contributed zero entries and must not be
    // patched in place.
    std::array<bool, 4> mcp_tier_unparseable_{false, false, false, false};
    // Whether load() ever populated the per-tier bookkeeping. The legacy
    // /config set path saves without loading (default settings); the §B
    // filter only runs after a real load.
    bool mcp_bookkeeping_loaded_ = false;
    // §A diagnostics: a given unparseable path warns at most once per
    // ConfigManager instance (a command path loads twice — initial load and
    // the forced post-mutation reload — but the warning is one per process).
    std::unordered_set<std::string> mcp_warned_paths_;

public:
    /// Initialize with default settings
    ConfigManager()
        : global_path_(default_global_config_path())
        , user_path_(cc::constants::paths::config_home_write() / "config.json")
        , project_path_(default_project_config_path())
        , local_path_(derive_local_config_path(default_project_config_path())) {}

    /// Initialize with explicit global + project paths (for testing). The
    /// user tier is absent (empty path, never probed); a local path is
    /// derived next to the project file and read when it exists.
    explicit ConfigManager(std::filesystem::path global, std::filesystem::path project)
        : global_path_(std::move(global))
        , project_path_(std::move(project))
        , local_path_(derive_local_config_path(project_path_)) {}

    /// Initialize with explicit paths for all four tiers (hermetic tests).
    ConfigManager(std::filesystem::path global,
                  std::filesystem::path user,
                  std::filesystem::path project,
                  std::filesystem::path local)
        : global_path_(std::move(global))
        , user_path_(std::move(user))
        , project_path_(std::move(project))
        , local_path_(std::move(local)) {}

    /// Load configuration from all sources, merging by priority
    [[nodiscard]] Result<void> load() {
        // Start with defaults
        settings_ = Settings{};

        // Reset the per-tier MCP bookkeeping used by overlay merge and §B.
        mcp_owner_.clear();
        for (auto& names : mcp_physical_names_) names.clear();
        for (auto& list : mcp_file_servers_) list.clear();
        mcp_tier_unparseable_.fill(false);
        mcp_bookkeeping_loaded_ = true;

        // RFC-0001 B followup c6: four physical tiers, lowest to highest.
        // global/project parse errors stay HARD failures (those files are
        // only ever written as JSON by Loom); user/local parse errors are
        // tolerated per §A.
        if (auto result = load_tier(global_path_, McpStorageScope::Global, false);
            !result && result.error().code != ErrorCode::ConfigNotFound) {
            return std::unexpected(result.error());
        }
        if (!user_path_.empty()) {
            if (auto result = load_tier(user_path_, McpStorageScope::User, true);
                !result && result.error().code != ErrorCode::ConfigNotFound) {
                return std::unexpected(result.error());
            }
        }
        if (auto result = load_tier(project_path_, McpStorageScope::Project, false);
            !result && result.error().code != ErrorCode::ConfigNotFound) {
            return std::unexpected(result.error());
        }
        if (auto result = load_tier(local_path_, McpStorageScope::Local, true);
            !result && result.error().code != ErrorCode::ConfigNotFound) {
            return std::unexpected(result.error());
        }

        // Layer: Environment variables (override file configs)
        apply_environment_variables();

        dirty_ = false;
        return {};
    }

    /// Save current settings to the specified config file
    [[nodiscard]] VoidResult save(ConfigSource target = ConfigSource::ProjectConfig) {
        const auto& path = (target == ConfigSource::GlobalConfig) ? global_path_ : project_path_;

        // Ensure parent directory exists
        auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return std::unexpected(Error::make(
                    ErrorCode::ConfigWriteError,
                    std::format("Failed to create config directory: {}", parent.string())
                ));
            }
        }

        // Serialize and write
        auto json = serialize_settings(target);
        std::ofstream file(path);
        if (!file.is_open()) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigWriteError,
                std::format("Cannot open config file for writing: {}", path.string())
            ));
        }

        file << json;
        dirty_ = false;
        return {};
    }

    /// Get the current effective settings (read-only)
    [[nodiscard]] const Settings& settings() const noexcept { return settings_; }

    /// Get mutable settings reference for modification
    [[nodiscard]] Settings& settings_mut() noexcept {
        dirty_ = true;
        return settings_;
    }

    /// Check if a feature flag is enabled
    [[nodiscard]] bool is_feature_enabled(FeatureFlag flag) const noexcept {
        return settings_.features.is_enabled(flag);
    }

    /// Set a feature flag at runtime
    void set_feature(FeatureFlag flag, bool enabled) {
        settings_.features.set(flag, enabled);
        dirty_ = true;
    }

    /// Get the API key (from settings or environment)
    [[nodiscard]] std::optional<std::string> api_key() const {
        if (settings_.network.api_key) return settings_.network.api_key;
        // Fallback: check environment (already applied during load)
        return std::nullopt;
    }

    /// Check if there are unsaved changes
    [[nodiscard]] bool is_dirty() const noexcept { return dirty_; }

    /// Get path to the global config file
    [[nodiscard]] const std::filesystem::path& global_config_path() const noexcept {
        return global_path_;
    }

    /// Get path to the project config file
    [[nodiscard]] const std::filesystem::path& project_config_path() const noexcept {
        return project_path_;
    }

    // ================================================================
    // MCP per-tier routing (RFC-0001 B followup c6)
    // ================================================================

    /// Parse a CLI scope label. "local"|"user"|"project" are the user-facing
    /// labels; "global" is accepted for internal use (scoped remove of the
    /// legacy tier).
    [[nodiscard]] static std::optional<McpStorageScope>
    mcp_scope_from_label(std::string_view label) noexcept {
        if (label == "local")   return McpStorageScope::Local;
        if (label == "user")    return McpStorageScope::User;
        if (label == "project") return McpStorageScope::Project;
        if (label == "global")  return McpStorageScope::Global;
        return std::nullopt;
    }

    /// The configured (scope, path) pairs in lowest-to-highest precedence.
    /// The user tier is omitted when its path is empty (2-arg ctor); missing
    /// files are included (callers tolerate absence).
    [[nodiscard]] std::vector<std::pair<McpStorageScope, std::filesystem::path>>
    mcp_scope_paths() const {
        std::vector<std::pair<McpStorageScope, std::filesystem::path>> result;
        result.emplace_back(McpStorageScope::Global, global_path_);
        if (!user_path_.empty()) result.emplace_back(McpStorageScope::User, user_path_);
        result.emplace_back(McpStorageScope::Project, project_path_);
        result.emplace_back(McpStorageScope::Local, local_path_);
        return result;
    }

    /// Insert-or-update one server in exactly the file backing `scope`.
    /// Never touches the other tiers. Against a user/local file that is not a
    /// parseable JSON object (§A) this fails with an actionable message and
    /// leaves the bytes untouched.
    [[nodiscard]] VoidResult
    upsert_mcp_server(McpStorageScope scope, const McpServerConfig& server) {
        const auto* path = path_for_scope(scope);
        if (path == nullptr || path->empty()) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigWriteError,
                std::format("No config file is configured for the '{}' scope",
                            mcp_scope_label(scope))));
        }
        if (mcp_tier_unparseable_[scope_index(scope)]) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigParseError,
                std::format("{} is not valid JSON; move it aside first "
                            "(use 'loom mcp' to edit MCP configuration)",
                            path->string())));
        }
        if (server.name.empty()) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput, "MCP server name must not be empty"));
        }

        const std::string fragment = serialize_server_object(server);
        const std::string name = server.name;
        auto patched = patch_mcp_file(*path,
            [&](cc::utils::json::JsonMutVal& servers,
                cc::utils::json::JsonMutDoc& doc) -> VoidResult {
                // raw_json strict-parses and deep-copies the fragment into
                // the patch document.
                auto fragment_val = doc.raw_json(fragment);
                // A null/invalid value makes yyjson_mut_obj_put DELETE the
                // key (yyjson.h) — reject before touching the object.
                if (!fragment_val.valid() || !fragment_val.is_obj()) {
                    return std::unexpected(Error::make(
                        ErrorCode::InternalError,
                        "Internal error: serialized MCP server entry is not valid JSON"));
                }
                servers.add(name, fragment_val);
                return {};
            });
        if (!patched) return std::unexpected(patched.error());

        // Keep in-memory bookkeeping consistent with the patched file so a
        // same-instance save() cannot resurrect/misfilter (B1).
        {
            auto& retained = mcp_file_servers_[scope_index(scope)];
            auto existing = std::ranges::find(retained, name, &McpServerConfig::name);
            if (existing != retained.end()) {
                *existing = server;
            } else {
                retained.push_back(server);
            }
            mcp_physical_names_[scope_index(scope)].insert(name);
            refresh_effective_mcp_entry(name);
        }
        // The .gitignore applier already ran inside patch_mcp_file for the
        // local tier (on every local patch, not just file creation).
        return {};
    }

    /// Best-effort removal across every tier file that physically contains
    /// `name` ON DISK (D3), or restricted to one tier when `scope` is given.
    /// Presence is verified from the files themselves rather than the
    /// post-load cache, so the call is re-runnable/idempotent within one
    /// instance. Returns touched/failed path lists; an absent name yields
    /// empty lists.
    [[nodiscard]] Result<McpRemoveOutcome>
    remove_mcp_server(std::string_view name,
                      std::optional<McpStorageScope> scope = std::nullopt) {
        McpRemoveOutcome outcome;
        std::vector<std::pair<McpStorageScope, std::filesystem::path>> candidates;
        if (scope) {
            if (const auto* path = path_for_scope(*scope);
                path != nullptr && !path->empty()) {
                candidates.emplace_back(*scope, *path);
            }
        } else {
            for (const auto& [candidate_scope, path] : mcp_scope_paths()) {
                candidates.emplace_back(candidate_scope, path);
            }
        }

        std::vector<McpStorageScope> changed_tiers;
        for (const auto& [candidate_scope, path] : candidates) {
            if (!file_contains_mcp_server(path, name)) {
                continue;
            }
            auto patched = patch_mcp_file(path,
                [&](cc::utils::json::JsonMutVal& servers,
                    cc::utils::json::JsonMutDoc&) -> VoidResult {
                    (void)servers.remove(name);
                    return {};
                });
            if (patched) {
                outcome.touched.push_back(path);
                const auto tier = scope_index(candidate_scope);
                mcp_physical_names_[tier].erase(std::string(name));
                std::erase_if(mcp_file_servers_[tier],
                    [&](const McpServerConfig& entry) { return entry.name == name; });
                changed_tiers.push_back(candidate_scope);
            } else {
                outcome.failed.push_back(path);
            }
        }
        // Recompute owner + merged entry once from the post-removal
        // physical sets; failed tiers still hold the name on disk and so
        // keep it effective. Without this, a same-instance save() would
        // resurrect the removed entry (B1).
        if (!changed_tiers.empty()) {
            refresh_effective_mcp_entry(std::string(name));
        }
        return outcome;
    }

    /// Whether a tier file currently has `name` as a key under mcpServers.
    /// Missing files and unparseable/non-object content report false (an
    /// unparseable tier cannot physically contain an MCP entry).
    [[nodiscard]] static bool
    file_contains_mcp_server(const std::filesystem::path& path,
                             std::string_view name) {
        if (path.empty()) return false;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) return false;
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        auto parsed = cc::utils::json::parse(content);
        if (!parsed) return false;
        auto root = parsed->root();
        if (!root.is_obj()) return false;
        auto servers = root.get("mcpServers");
        return servers.is_obj() && servers.has(name);
    }

    /// Rebuild the effective (merged) entry for one name from the retained
    /// per-tier values after a disk mutation. Recomputes the highest-owner
    /// map; when no tier physically holds the name anymore it is dropped
    /// from the merged vector AND the owner map. This is what keeps a
    /// same-instance save() from resurrecting a just-removed entry or
    /// misapplying the §B filter.
    ///
    /// Position policy: if the name still exists in the merged vector when
    /// this is called (an upsert/disable of an effective entry, or removal
    /// from a tier that is NOT the last holder), the rebuilt entry reuses
    /// its prior index. After a remove that dropped the effective entry, a
    /// same-instance re-add (no reload) APPENDS — the saved position is
    /// then end-of-vector. Every production flow reloads before readding,
    /// so this edge is unobservable outside that one in-process sequence.
    void refresh_effective_mcp_entry(const std::string& name) {
        std::size_t position = settings_.mcp_servers.size();
        for (std::size_t i = 0; i < settings_.mcp_servers.size(); ++i) {
            if (settings_.mcp_servers[i].name == name) {
                position = i;
                break;
            }
        }
        std::erase_if(settings_.mcp_servers,
            [&](const McpServerConfig& entry) { return entry.name == name; });

        for (int tier = 3; tier >= 0; --tier) {
            auto& retained = mcp_file_servers_[static_cast<std::size_t>(tier)];
            auto it = std::ranges::find(retained, name, &McpServerConfig::name);
            if (it == retained.end()) continue;
            auto inserted = settings_.mcp_servers.insert(
                settings_.mcp_servers.begin() +
                    std::min(position, settings_.mcp_servers.size()),
                *it);
            (void)inserted;
            mcp_owner_[name] = static_cast<McpStorageScope>(tier);
            return;
        }
        mcp_owner_.erase(name);
    }

    /// Set only the "disabled" flag on the entry, in place, preserving every
    /// sibling and unknown key. With no scope the highest-precedence owner
    /// file is patched (a global-only entry therefore patches the legacy
    /// global file, per D5); with a scope, that file alone is patched and an
    /// absent entry there is an error.
    [[nodiscard]] VoidResult
    set_mcp_server_disabled(std::string_view name, bool disabled,
                            std::optional<McpStorageScope> scope = std::nullopt) {
        McpStorageScope target_scope = McpStorageScope::Global;
        if (scope) {
            target_scope = *scope;
            if (!mcp_physical_names_[scope_index(target_scope)]
                    .contains(std::string(name))) {
                return std::unexpected(Error::make(
                    ErrorCode::NotFound,
                    std::format("MCP server '{}' not found in the {} config",
                                name, mcp_scope_label(target_scope))));
            }
        } else {
            auto owner = mcp_owner_.find(std::string(name));
            if (owner == mcp_owner_.end()) {
                return std::unexpected(Error::make(
                    ErrorCode::NotFound,
                    std::format("MCP server '{}' not found", name)));
            }
            target_scope = owner->second;
        }

        const auto* path = path_for_scope(target_scope);
        if (path == nullptr || path->empty()) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigWriteError,
                std::format("No config file is configured for the '{}' scope",
                            mcp_scope_label(target_scope))));
        }
        const std::string owned_name(name);
        return set_mcp_servers_disabled_in(target_scope,
                                           std::span<const std::string>(&owned_name, 1),
                                           disabled);
    }

    /// Set only the "disabled" flag on several entries that all live in the
    /// SAME tier file, in one read-modify-write. Used by "mcp enable/disable
    /// all" so each distinct owner file is patched exactly once; sibling and
    /// unknown keys are preserved.
    [[nodiscard]] VoidResult
    set_mcp_servers_disabled_in(McpStorageScope scope,
                                std::span<const std::string> names,
                                bool disabled) {
        const auto* path = path_for_scope(scope);
        if (path == nullptr || path->empty()) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigWriteError,
                std::format("No config file is configured for the '{}' scope",
                            mcp_scope_label(scope))));
        }
        auto patched = patch_mcp_file(*path,
            [&](cc::utils::json::JsonMutVal& servers,
                cc::utils::json::JsonMutDoc&) -> VoidResult {
                for (const auto& name : names) {
                    auto entry = servers.get(name);
                    if (!entry.is_obj()) {
                        return std::unexpected(Error::make(
                            ErrorCode::ConfigParseError,
                            std::format("MCP server '{}' entry is not a JSON object in {}",
                                        name, path->string())));
                    }
                    entry.set("disabled", disabled);
                }
                return {};
            });
        if (!patched) return std::unexpected(patched.error());

        // Keep retained values + the effective merged entry consistent with
        // the patched file (only the highest-tier owner is effective; lower
        // shadow copies stay invisible until the higher one is removed).
        auto& retained = mcp_file_servers_[scope_index(scope)];
        for (const auto& name : names) {
            auto existing = std::ranges::find(retained, name, &McpServerConfig::name);
            if (existing != retained.end()) {
                existing->disabled = disabled;
            }
            refresh_effective_mcp_entry(name);
        }
        return {};
    }

    /// Every configured tier file that physically contains `name`, in
    /// lowest-to-highest precedence order (based on the last load()).
    [[nodiscard]] std::vector<std::pair<McpStorageScope, std::filesystem::path>>
    find_mcp_server_files(std::string_view name) const {
        std::vector<std::pair<McpStorageScope, std::filesystem::path>> result;
        for (const auto& [scope, path] : mcp_scope_paths()) {
            if (mcp_physical_names_[scope_index(scope)]
                    .contains(std::string(name))) {
                result.emplace_back(scope, path);
            }
        }
        return result;
    }

    /// Highest-precedence tier owning `name` after the last load(), or nullopt.
    [[nodiscard]] std::optional<McpStorageScope>
    mcp_server_owner(std::string_view name) const {
        auto found = mcp_owner_.find(std::string(name));
        if (found == mcp_owner_.end()) return std::nullopt;
        return found->second;
    }

private:
    /// Load one physical config tier. Missing files always surface
    /// ConfigNotFound (tolerated by load()). A parse failure / non-object
    /// root is a hard error for global/project; for user/local (soft_tier)
    /// the tier contributes zero entries plus one stderr diagnostic and
    /// loading continues (§A).
    [[nodiscard]] VoidResult
    load_tier(const std::filesystem::path& path, McpStorageScope scope, bool soft_tier) {
        if (path.empty()) return {};
        if (!std::filesystem::exists(path)) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigNotFound,
                std::format("Config file not found: {}", path.string())
            ));
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::unexpected(Error::make(
                ErrorCode::ConfigParseError,
                std::format("Cannot open config file: {}", path.string())
            ));
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        auto doc_result = cc::utils::json::parse(content);
        if (!doc_result || !doc_result->root().is_obj()) {
            // A zero-length / all-whitespace user/local file is treated as
            // MISSING: the tier contributes nothing, emits no warning, and a
            // later upsert creates it fresh (rather than rejecting it as
            // invalid JSON). Garbage/non-object content keeps the §A policy.
            const bool blank =
                content.find_first_not_of(" \t\r\n") == std::string::npos;
            if (soft_tier && blank) {
                return std::unexpected(Error::make(
                    ErrorCode::ConfigNotFound,
                    std::format("Config file is empty: {}", path.string())
                ));
            }
            if (soft_tier) {
                mcp_tier_unparseable_[scope_index(scope)] = true;
                // One diagnostic per path per instance: the CLI command
                // path loads twice (initial + post-mutation reload).
                if (mcp_warned_paths_.insert(path.string()).second) {
                    std::println(stderr,
                        "warning: ignoring {}: not valid JSON "
                        "(use 'loom mcp' to edit MCP configuration)",
                        path.string());
                }
                return {};
            }
            return std::unexpected(Error::make(
                ErrorCode::ConfigParseError,
                doc_result
                    ? std::string("Config JSON root must be an object")
                    : std::string("Failed to parse config JSON: ") +
                          doc_result.error().message()
            ));
        }

        return merge_parsed(doc_result->root(), scope);
    }

    /// Parse JSON content and merge into current settings.
    /// Uses yyjson via cc::utils::json to deserialize fields.
    [[nodiscard]] VoidResult
    merge_parsed(cc::utils::json::JsonVal root, McpStorageScope scope) {
        // Model settings
        if (auto model = root.get("model"); model.is_obj()) {
            if (auto v = model.get("default_model"); v.is_str()) {
                settings_.model.default_model = std::string(v.as_str());
            }
            if (auto v = model.get("max_output_tokens"); v.is_num()) {
                settings_.model.max_output_tokens = static_cast<std::uint32_t>(v.as_int());
            }
            if (auto v = model.get("temperature"); v.is_num()) {
                settings_.model.temperature = v.as_double();
            }
            if (auto v = model.get("extended_thinking"); v.is_bool()) {
                settings_.model.extended_thinking = v.as_bool();
            }
            if (auto v = model.get("thinking_budget"); v.is_num()) {
                settings_.model.thinking_budget = static_cast<std::uint32_t>(v.as_int());
            }
            if (auto v = model.get("context_window_size"); v.is_num()) {
                settings_.model.context_window_size = static_cast<std::uint32_t>(v.as_int());
            }
        }

        // Display settings
        if (auto display = root.get("display"); display.is_obj()) {
            if (auto v = display.get("show_thinking"); v.is_bool()) {
                settings_.display.show_thinking = v.as_bool();
            }
            if (auto v = display.get("show_token_usage"); v.is_bool()) {
                settings_.display.show_token_usage = v.as_bool();
            }
            if (auto v = display.get("compact_mode"); v.is_bool()) {
                settings_.display.compact_mode = v.as_bool();
            }
            if (auto v = display.get("line_width"); v.is_num()) {
                settings_.display.line_width = static_cast<std::uint32_t>(v.as_int());
            }
            if (auto v = display.get("theme"); v.is_str()) {
                settings_.display.theme = std::string(v.as_str());
            }
        }

        // Network settings
        if (auto network = root.get("network"); network.is_obj()) {
            if (auto v = network.get("api_key"); v.is_str()) {
                settings_.network.api_key = std::string(v.as_str());
            }
            if (auto v = network.get("base_url"); v.is_str()) {
                settings_.network.base_url = std::string(v.as_str());
            }
            if (auto v = network.get("proxy"); v.is_str()) {
                settings_.network.proxy = std::string(v.as_str());
            }
            if (auto v = network.get("timeout_seconds"); v.is_num()) {
                settings_.network.timeout_seconds = static_cast<std::uint32_t>(v.as_int());
            }
            if (auto v = network.get("max_retries"); v.is_num()) {
                settings_.network.max_retries = static_cast<std::uint32_t>(v.as_int());
            }
            if (auto v = network.get("verify_ssl"); v.is_bool()) {
                settings_.network.verify_ssl = v.as_bool();
            }
        }

        // Permission settings
        if (auto perms = root.get("permissions"); perms.is_obj()) {
            if (auto v = perms.get("allow_bash"); v.is_bool()) {
                settings_.permissions.allow_bash = v.as_bool();
            }
            if (auto v = perms.get("allow_file_write"); v.is_bool()) {
                settings_.permissions.allow_file_write = v.as_bool();
            }
            if (auto v = perms.get("allow_network"); v.is_bool()) {
                settings_.permissions.allow_network = v.as_bool();
            }
            if (auto arr = perms.get("allowed_paths"); arr.is_arr()) {
                settings_.permissions.allowed_paths.clear();
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    if (auto item = arr.at(i); item.is_str()) {
                        settings_.permissions.allowed_paths.emplace_back(item.as_str());
                    }
                }
            }
            if (auto arr = perms.get("denied_paths"); arr.is_arr()) {
                settings_.permissions.denied_paths.clear();
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    if (auto item = arr.at(i); item.is_str()) {
                        settings_.permissions.denied_paths.emplace_back(item.as_str());
                    }
                }
            }
            if (auto arr = perms.get("allowed_commands"); arr.is_arr()) {
                settings_.permissions.allowed_commands.clear();
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    if (auto item = arr.at(i); item.is_str()) {
                        settings_.permissions.allowed_commands.emplace_back(item.as_str());
                    }
                }
            }
            // permissions.deny — raw rules surfaced to the engine pre-request
            // tool filter (TS permissions.ts alwaysDenyRules). Non-string
            // elements are skipped defensively.
            if (auto deny = perms.get("deny"); deny.is_arr()) {
                settings_.permissions.deny_rules.clear();
                for (std::size_t i = 0; i < deny.size(); ++i) {
                    if (auto item = deny.at(i); item.is_str()) {
                        settings_.permissions.deny_rules.emplace_back(item.as_str());
                    }
                }
            }
        }

        // MCP servers. D2 (RFC-0001 B followup c6): per-entry NAME overlay
        // across the four physical files. Each file's entries replace
        // same-named merged entries (erase, then append); an empty
        // mcpServers object overrides NOTHING. The file's own parsed values
        // are retained separately for the §B full-save secret boundary.
        if (auto servers = root.get("mcpServers"); servers.is_obj()) {
            const auto tier = scope_index(scope);
            servers.iter_obj([&](auto key, auto val) {
                if (!key.is_str() || !val.is_obj()) return;

                McpServerConfig cfg;
                cfg.name = std::string(key.as_str());
                cfg.transport = json_string(val, "type")
                    .or_else([&] { return json_string(val, "transport"); })
                    .value_or(val.get("url").is_str() ? std::string("http") : std::string("stdio"));
                if (auto cmd = val.get("command"); cmd.is_str()) {
                    cfg.command = std::string(cmd.as_str());
                }
                if (auto url = val.get("url"); url.is_str()) {
                    cfg.url = std::string(url.as_str());
                }
                if (auto args = val.get("args"); args.is_arr()) {
                    for (std::size_t j = 0; j < args.size(); ++j) {
                        if (auto arg = args.at(j); arg.is_str()) {
                            cfg.args.emplace_back(arg.as_str());
                        }
                    }
                }
                if (auto env = val.get("env"); env.is_obj()) {
                    env.iter_obj([&](auto ek, auto ev) {
                        if (ek.is_str() && ev.is_str()) {
                            cfg.env[std::string(ek.as_str())] = std::string(ev.as_str());
                        }
                    });
                }
                if (auto headers = val.get("headers"); headers.is_obj()) {
                    headers.iter_obj([&](auto hk, auto hv) {
                        if (hk.is_str() && hv.is_str()) {
                            cfg.headers[std::string(hk.as_str())] = std::string(hv.as_str());
                        }
                    });
                }
                cfg.headers_helper = json_string(val, "headersHelper")
                    .or_else([&] { return json_string(val, "headers_helper"); });
                // disabled is optional<bool>: an explicit JSON false must be
                // captured, while an absent key stays nullopt.
                if (auto disabled = val.get("disabled"); disabled.is_bool()) {
                    cfg.disabled = disabled.as_bool();
                }
                cfg.config_scope = json_string(val, "configScope")
                    .or_else([&] { return json_string(val, "config_scope"); })
                    .value_or("project");
                if (auto oauth = val.get("oauth"); oauth.is_obj()) {
                    McpOAuthConfig oauth_cfg;
                    oauth_cfg.auth_server_metadata_url = json_string(oauth, "authServerMetadataUrl")
                        .or_else([&] { return json_string(oauth, "auth_server_metadata_url"); });
                    if (auto callback_port = oauth.get("callbackPort"); callback_port.is_num()) {
                        oauth_cfg.callback_port = static_cast<int>(callback_port.as_int());
                    } else if (auto callback_port = oauth.get("callback_port"); callback_port.is_num()) {
                        oauth_cfg.callback_port = static_cast<int>(callback_port.as_int());
                    }
                    oauth_cfg.client_id = json_string(oauth, "clientId")
                        .or_else([&] { return json_string(oauth, "client_id"); });
                    if (auto xaa = oauth.get("xaa"); xaa.is_bool()) {
                        oauth_cfg.xaa = xaa.as_bool();
                    }
                    oauth_cfg.issuer = json_string(oauth, "issuer");
                    cfg.oauth = std::move(oauth_cfg);
                }
                // Overlay: drop any same-named entry from lower tiers, then
                // append this file's value as the effective one.
                std::erase_if(settings_.mcp_servers,
                    [&](const McpServerConfig& existing) {
                        return existing.name == cfg.name;
                    });
                settings_.mcp_servers.push_back(cfg);
                // Retain this file's OWN value and ownership data for §B.
                mcp_file_servers_[tier].push_back(cfg);
                mcp_physical_names_[tier].insert(cfg.name);
                mcp_owner_[cfg.name] = scope;
            });
        }

        // System prompt
        if (auto v = root.get("systemPrompt"); v.is_str()) {
            settings_.system_prompt = std::string(v.as_str());
        }

        // Custom instructions
        if (auto arr = root.get("customInstructions"); arr.is_arr()) {
            settings_.custom_instructions.clear();
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (auto item = arr.at(i); item.is_str()) {
                    settings_.custom_instructions.emplace_back(item.as_str());
                }
            }
        }

        // Feature flags (as integer bitmask or object)
        if (auto v = root.get("features"); v.is_num()) {
            settings_.features = FeatureFlags(static_cast<std::uint32_t>(v.as_int()));
        }

        return {};
    }

    /// Apply environment variables to settings (highest override priority)
    void apply_environment_variables() {
        // ANTHROPIC_API_KEY -> network.api_key
        if (auto* val = std::getenv("ANTHROPIC_API_KEY")) {
            settings_.network.api_key = val;
        }

        // ANTHROPIC_BASE_URL -> network.base_url
        if (auto* val = std::getenv("ANTHROPIC_BASE_URL")) {
            settings_.network.base_url = val;
        }

        // LOOM_MODEL -> model.default_model
        if (auto* val = std::getenv("LOOM_MODEL")) {
            settings_.model.default_model = val;
        }

        // HTTPS_PROXY / HTTP_PROXY -> network.proxy
        if (auto* val = std::getenv("HTTPS_PROXY")) {
            settings_.network.proxy = val;
        } else if (auto* val2 = std::getenv("HTTP_PROXY")) {
            settings_.network.proxy = val2;
        }

        // LOOM_MAX_TOKENS -> model.max_output_tokens
        if (auto* val = std::getenv("LOOM_MAX_TOKENS")) {
            try {
                settings_.model.max_output_tokens = static_cast<std::uint32_t>(std::stoul(val));
            } catch (...) {
                // Ignore invalid values
            }
        }
    }

    /// Serialize settings to JSON string. When writing the PROJECT file after
    /// a real load, mcpServers go through the §B secret-boundary filter.
    [[nodiscard]] std::string serialize_settings(
        ConfigSource target = ConfigSource::ProjectConfig) const {
        const std::vector<const McpServerConfig*> mcp_to_emit =
            mcp_servers_for_save(target);
        std::string json;
        json += "{\n";
        json += "  \"model\": {\n";
        json += std::format("    \"default_model\": \"{}\",\n", escape_json(settings_.model.default_model));
        json += std::format("    \"max_output_tokens\": {},\n", settings_.model.max_output_tokens);
        json += std::format("    \"extended_thinking\": {},\n", settings_.model.extended_thinking ? "true" : "false");
        json += std::format("    \"context_window_size\": {}\n", settings_.model.context_window_size);
        json += "  },\n";
        json += "  \"display\": {\n";
        json += std::format("    \"show_thinking\": {},\n", settings_.display.show_thinking ? "true" : "false");
        json += std::format("    \"show_token_usage\": {},\n", settings_.display.show_token_usage ? "true" : "false");
        json += std::format("    \"compact_mode\": {},\n", settings_.display.compact_mode ? "true" : "false");
        json += std::format("    \"theme\": \"{}\"\n", escape_json(settings_.display.theme));
        json += "  },\n";
        json += "  \"network\": {\n";
        json += std::format("    \"timeout_seconds\": {},\n", settings_.network.timeout_seconds);
        json += std::format("    \"max_retries\": {},\n", settings_.network.max_retries);
        json += std::format("    \"verify_ssl\": {}\n", settings_.network.verify_ssl ? "true" : "false");
        json += "  },\n";
        json += std::format("  \"features\": {},\n", settings_.features.raw());
        json += "  \"mcpServers\": ";
        append_mcp_servers(json, mcp_to_emit);

        if (settings_.system_prompt) {
            json += std::format(",\n  \"systemPrompt\": \"{}\"", escape_json(*settings_.system_prompt));
        }
        if (!settings_.custom_instructions.empty()) {
            json += ",\n  \"customInstructions\": ";
            append_string_array(json, settings_.custom_instructions);
        }
        json += "\n}\n";
        return json;
    }

    [[nodiscard]] static std::string escape_json(std::string_view value) {
        std::string out;
        out.reserve(value.size());
        for (char ch : value) {
            switch (ch) {
                case '\\': out += R"(\\)"; break;
                case '"':  out += R"(\")"; break;
                case '\b': out += R"(\b)"; break;
                case '\f': out += R"(\f)"; break;
                case '\n': out += R"(\n)"; break;
                case '\r': out += R"(\r)"; break;
                case '\t': out += R"(\t)"; break;
                default:   out += ch; break;
            }
        }
        return out;
    }

    static void append_string_array(std::string& json, const std::vector<std::string>& values) {
        json += "[";
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) json += ", ";
            json += std::format("\"{}\"", escape_json(values[i]));
        }
        json += "]";
    }

    static void append_string_map(
        std::string& json,
        const std::unordered_map<std::string, std::string>& values
    ) {
        json += "{";
        std::size_t index = 0;
        for (const auto& [key, value] : values) {
            if (index++ > 0) json += ", ";
            json += std::format("\"{}\": \"{}\"", escape_json(key), escape_json(value));
        }
        json += "}";
    }

    /// §B (RFC-0001 B followup c6): choose which merged MCP entries a full
    /// save to `target` may emit.
    ///
    /// PROJECT save emits a merged entry E iff (a) the name is PHYSICALLY
    /// present in the project file — in which case THAT FILE'S OWN parsed
    /// value is emitted, never the shadowing higher-tier merged value (a
    /// user/local shadow can carry Authorization headers and must never
    /// enter the tracked file) — or (b) the name's highest owner is global
    /// or project (preserves the pre-existing global→project copy-down; it
    /// is Tier-2 cleanup, not widened). Entries owned solely by user/local
    /// and absent from the project file are omitted.
    ///
    /// GLOBAL save keeps the legacy full-merged behavior (no production
    /// callers). Without any prior load() the per-tier data does not exist;
    /// the legacy /config set path lands here and keeps legacy behavior.
    [[nodiscard]] std::vector<const McpServerConfig*>
    mcp_servers_for_save(ConfigSource target) const {
        std::vector<const McpServerConfig*> result;
        if (target != ConfigSource::ProjectConfig || !mcp_bookkeeping_loaded_) {
            for (const auto& server : settings_.mcp_servers) result.push_back(&server);
            return result;
        }
        const auto project_idx = scope_index(McpStorageScope::Project);
        for (const auto& server : settings_.mcp_servers) {
            if (mcp_physical_names_[project_idx].contains(server.name)) {
                const McpServerConfig* own = nullptr;
                for (const auto& project_server : mcp_file_servers_[project_idx]) {
                    if (project_server.name == server.name) {
                        own = &project_server;
                        break;
                    }
                }
                result.push_back(own != nullptr ? own : &server);
                continue;
            }
            auto owner = mcp_owner_.find(server.name);
            if (owner != mcp_owner_.end() &&
                (owner->second == McpStorageScope::Global ||
                 owner->second == McpStorageScope::Project)) {
                result.push_back(&server);
            }
            // user/local-only: omitted.
        }
        return result;
    }

    /// Serialize one MCP server to a BARE, strict-JSON-parseable object
    /// fragment (no quoted name key, no envelope indentation). Key set and
    /// order are exactly what C4 pinned:
    /// type, command, args, env, url, headers, headersHelper, disabled,
    /// configScope, oauth{authServerMetadataUrl, callbackPort, clientId,
    /// xaa, issuer}. Base indentation is two spaces; append_mcp_servers is
    /// the pretty-printer that re-indents it for full saves, and the per-tier
    /// patcher feeds this string to JsonMutDoc::raw_json.
    [[nodiscard]] static std::string serialize_server_object(const McpServerConfig& server) {
        std::string json = "{\n";
        bool wrote_field = false;
        auto add_field = [&](std::string field) {
            if (wrote_field) json += ",\n";
            json += "  ";
            json += field;
            wrote_field = true;
        };

        add_field(std::format("\"type\": \"{}\"",
            escape_json(server.transport.empty() ? std::string_view("stdio") : std::string_view(server.transport))));
        if (!server.command.empty()) {
            add_field(std::format("\"command\": \"{}\"", escape_json(server.command)));
        }
        if (!server.args.empty()) {
            std::string args_json = "\"args\": ";
            append_string_array(args_json, server.args);
            add_field(std::move(args_json));
        }
        if (!server.env.empty()) {
            std::string env_json = "\"env\": ";
            append_string_map(env_json, server.env);
            add_field(std::move(env_json));
        }
        if (server.url) {
            add_field(std::format("\"url\": \"{}\"", escape_json(*server.url)));
        }
        if (!server.headers.empty()) {
            std::string headers_json = "\"headers\": ";
            append_string_map(headers_json, server.headers);
            add_field(std::move(headers_json));
        }
        if (server.headers_helper) {
            add_field(std::format("\"headersHelper\": \"{}\"", escape_json(*server.headers_helper)));
        }
        // disabled is optional<bool>: omit when unset so "absent"
        // round-trips; explicit true/false is always written.
        if (server.disabled.has_value()) {
            add_field(std::format("\"disabled\": {}", *server.disabled ? "true" : "false"));
        }
        // configScope is a non-optional std::string ("project" default):
        // always written in canonical camelCase, even when defaulted.
        add_field(std::format("\"configScope\": \"{}\"", escape_json(server.config_scope)));
        if (server.oauth) {
            std::string oauth_json = "\"oauth\": {";
            bool wrote_oauth = false;
            auto add_oauth = [&](std::string field) {
                if (wrote_oauth) oauth_json += ", ";
                oauth_json += field;
                wrote_oauth = true;
            };
            if (server.oauth->auth_server_metadata_url) {
                add_oauth(std::format("\"authServerMetadataUrl\": \"{}\"",
                    escape_json(*server.oauth->auth_server_metadata_url)));
            }
            if (server.oauth->callback_port) {
                add_oauth(std::format("\"callbackPort\": {}", *server.oauth->callback_port));
            }
            if (server.oauth->client_id) {
                add_oauth(std::format("\"clientId\": \"{}\"", escape_json(*server.oauth->client_id)));
            }
            if (server.oauth->xaa) {
                add_oauth("\"xaa\": true");
            }
            if (server.oauth->issuer) {
                add_oauth(std::format("\"issuer\": \"{}\"", escape_json(*server.oauth->issuer)));
            }
            oauth_json += "}";
            add_field(std::move(oauth_json));
        }
        json += "\n}";
        return json;
    }

    /// Pretty-print MCP servers in the historical full-save envelope. The
    /// per-entry field text comes solely from serialize_server_object (one
    /// logical wire shape); its 2-space base indentation is re-wrapped to
    /// the 4/6-space envelope so the full-save wire shape is byte-identical
    /// to the pre-C6 hand serializer.
    static void append_mcp_servers(
        std::string& json,
        const std::vector<const McpServerConfig*>& servers
    ) {
        json += "{";
        if (!servers.empty()) json += "\n";
        for (std::size_t i = 0; i < servers.size(); ++i) {
            const auto& server = *servers[i];
            json += std::format("    \"{}\": ", escape_json(server.name));
            const std::string fragment = serialize_server_object(server);
            std::size_t start = 0;
            bool first_line = true;
            while (true) {
                const std::size_t nl = fragment.find('\n', start);
                const std::string_view line(
                    fragment.data() + start,
                    (nl == std::string::npos ? fragment.size() : nl) - start);
                if (first_line) {
                    json += line;  // opening "{"
                    first_line = false;
                } else if (line == "}") {
                    json += "\n    }";
                } else {
                    // Field lines carry the 2-space base indentation.
                    json += "\n      ";
                    json += line.substr(2);
                }
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            if (i + 1 < servers.size()) json += ",";
            json += "\n";
        }
        json += servers.empty() ? "}" : "  }";
    }

    [[nodiscard]] static std::optional<std::string> json_string(
        cc::utils::json::JsonVal value,
        std::string_view key
    ) {
        auto child = value.get(key);
        if (!child.is_str()) return std::nullopt;
        return std::string(child.as_str());
    }

    /// Get default global config path (~/.config/loom/config.json)
    [[nodiscard]] static std::filesystem::path default_global_config_path() {
        if (auto* home = std::getenv("HOME")) {
            return std::filesystem::path(home) / ".config" / "loom" / "config.json";
        }
        return "config.json";  // Fallback
    }

    /// Get default project config path (.loom/config.json in cwd)
    [[nodiscard]] static std::filesystem::path default_project_config_path() {
        return std::filesystem::current_path() / ".loom" / "config.json";
    }

    /// Local companion of the project file: "<parent>/<stem>.local.json".
    /// Production: .loom/config.json -> .loom/config.local.json;
    /// 2-arg test ctor: root/project.json -> root/project.local.json.
    [[nodiscard]] static std::filesystem::path
    derive_local_config_path(const std::filesystem::path& project_path) {
        return project_path.parent_path() /
               (project_path.stem().string() + ".local.json");
    }

    [[nodiscard]] static std::size_t scope_index(McpStorageScope scope) noexcept {
        return static_cast<std::size_t>(scope);
    }

    [[nodiscard]] static std::string_view mcp_scope_label(McpStorageScope scope) noexcept {
        switch (scope) {
            case McpStorageScope::Global:  return "global";
            case McpStorageScope::User:    return "user";
            case McpStorageScope::Project: return "project";
            case McpStorageScope::Local:   return "local";
        }
        return "unknown";
    }

    [[nodiscard]] const std::filesystem::path*
    path_for_scope(McpStorageScope scope) const noexcept {
        switch (scope) {
            case McpStorageScope::Global:  return &global_path_;
            case McpStorageScope::User:    return &user_path_;
            case McpStorageScope::Project: return &project_path_;
            case McpStorageScope::Local:   return &local_path_;
        }
        return nullptr;
    }

    /// Mutator receives the mcpServers object (created if absent) and the
    /// owning mutable document (needed to embed raw_json fragments). A
    /// returned error aborts the write; the file bytes stay untouched.
    using McpPatchFn = std::function<VoidResult(
        cc::utils::json::JsonMutVal&, cc::utils::json::JsonMutDoc&)>;

    /// Read-modify-write one MCP tier file. A missing file is treated as
    /// `{}`. The whole document is strict-parsed (a parse failure or a
    /// non-object root is an error — the bytes are never overwritten), the
    /// mcpServers object is ensured, the mutator applied, an emptied
    /// mcpServers object dropped (other top-level sections preserved), and
    /// the result written atomically via a fixed tmp name + rename.
    [[nodiscard]] VoidResult
    patch_mcp_file(const std::filesystem::path& path, const McpPatchFn& mutator) {
        cc::utils::json::JsonMutDoc doc;
        cc::utils::json::JsonMutVal root;
        bool file_present = std::filesystem::exists(path);
        bool blank = false;
        std::string content;
        if (file_present) {
            std::ifstream file(path);
            if (!file.is_open()) {
                return std::unexpected(Error::make(
                    ErrorCode::ConfigWriteError,
                    std::format("Cannot open config file for editing: {}", path.string())));
            }
            content.assign((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
            blank = content.find_first_not_of(" \t\r\n") == std::string::npos;
            if (!blank) {
                auto parsed = cc::utils::json::parse(content);
                if (!parsed || !parsed->root().is_obj()) {
                    return std::unexpected(Error::make(
                        ErrorCode::ConfigParseError,
                        std::format("{} is not valid JSON; move it aside first "
                                    "(use 'loom mcp' to edit MCP configuration)",
                                    path.string())));
                }
                root = doc.copy_val(parsed->root());
                doc.set_root(root);
            }
        }
        if (!file_present || blank) {
            // Missing file or a zero-length/all-whitespace file: start from
            // {} and let the patch create it fresh.
            root = doc.object();
            doc.set_root(root);
        }

        auto servers = root.ensure_object("mcpServers");
        if (auto mutated = mutator(servers, doc); !mutated) {
            return std::unexpected(mutated.error());
        }
        // Drop an emptied mcpServers object; keep other sections untouched.
        if (servers.is_obj() && servers.size() == 0) {
            (void)root.remove("mcpServers");
        }

        // The LOCAL tier is the documented secret holder (it shadows tracked
        // files precisely to keep Authorization headers out of VCS): create
        // it owner-only, like ssh-style secret files. Applied to the tmp
        // BEFORE rename so the final file never briefly exists world-readable.
        const bool target_is_local = (path == local_path_);
        if (auto written = write_config_file_atomic(
                path, doc.to_pretty_string(), /*owner_only=*/target_is_local);
            !written) {
            return std::unexpected(written.error());
        }
        // Protect the local tier on EVERY patch, not just when this process
        // created it: a hand-created non-empty config.local.json must end up
        // gitignored too. The applier is idempotent (no-op on a present
        // entry) and never fatal.
        if (target_is_local) {
            ensure_local_config_gitignored();
        }
        return {};
    }

    /// Atomic write mirroring settings_manager's private template: fixed
    /// `<path>.tmp` + rename, no fsync (concurrent CLIs in one CWD can
    /// collide on the tmp name — known limitation copied by design).
    ///
    /// Permissions on the replacement file:
    /// - `owner_only` (the LOCAL tier): the tmp is forced to 0600 before
    ///   the rename; a chmod failure warns but is non-fatal.
    /// - otherwise, when replacing an EXISTING file, the tmp inherits that
    ///   file's mode (tmp+rename would otherwise reset a 0600/0640 file to
    ///   the umask default); any status/chmod error falls back to default
    ///   behavior. A NEW file keeps the default umask mode.
    [[nodiscard]] static VoidResult
    write_config_file_atomic(const std::filesystem::path& path,
                             const std::string& content,
                             bool owner_only = false) {
        auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return std::unexpected(Error::make(
                    ErrorCode::ConfigWriteError,
                    std::format("Failed to create config directory: {}", parent.string())));
            }
        }
        // Capture the existing file's mode before the tmp replaces it.
        std::error_code status_ec;
        const auto existing_status = std::filesystem::status(path, status_ec);
        const bool preserve_mode = !owner_only && !status_ec &&
            existing_status.type() != std::filesystem::file_type::not_found;
        const auto existing_perms = preserve_mode
            ? (existing_status.permissions() & std::filesystem::perms::mask)
            : std::filesystem::perms::unknown;
        auto temp = path;
        temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::trunc);
            if (!file.is_open()) {
                return std::unexpected(Error::make(
                    ErrorCode::ConfigWriteError,
                    std::format("Cannot open config file for writing: {}", path.string())));
            }
            file << content;
            file.close();
            if (!file) {
                std::error_code ec;
                std::filesystem::remove(temp, ec);
                return std::unexpected(Error::make(
                    ErrorCode::ConfigWriteError,
                    std::format("Failed to write config file: {}", path.string())));
            }
        }
        if (owner_only) {
            std::error_code perm_ec;
            std::filesystem::permissions(
                temp,
                std::filesystem::perms::owner_read |
                    std::filesystem::perms::owner_write,
                std::filesystem::perm_options::replace,
                perm_ec);
            // Non-fatal: the write itself succeeded; warn once via stderr
            // rather than discarding the configuration.
            if (perm_ec) {
                std::println(stderr,
                    "warning: could not restrict permissions on {}: {}",
                    path.string(), perm_ec.message());
            }
        } else if (preserve_mode) {
            // Keep the pre-existing inode's mode across the tmp+rename;
            // on failure leave the tmp at its umask default.
            std::error_code perm_ec;
            std::filesystem::permissions(
                temp, existing_perms,
                std::filesystem::perm_options::replace,
                perm_ec);
            (void)perm_ec;
        }
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            std::error_code remove_ec;
            std::filesystem::remove(temp, remove_ec);
            return std::unexpected(Error::make(
                ErrorCode::ConfigWriteError,
                std::format("Failed to replace config file: {}", path.string())));
        }
        return {};
    }

    /// First Local write: append the resolved local basename (e.g.
    /// config.local.json) to the workspace-root .gitignore. The pattern
    /// matches the file at any depth, so .loom/config.local.json is
    /// ignored. Mirrors settings_manager: idempotent, copes with a missing
    /// trailing newline; a missing/unwritable .gitignore is non-fatal.
    void ensure_local_config_gitignored() const {
        const auto gitignore = std::filesystem::current_path() / ".gitignore";
        const std::string entry = local_path_.filename().string();
        std::string content;
        std::error_code ec;
        if (std::filesystem::exists(gitignore, ec)) {
            std::ifstream in(gitignore);
            content.assign(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
            if (content.find(entry) != std::string::npos) return;
        }
        std::ofstream out(gitignore, std::ios::app);
        if (!out.is_open()) return;
        if (!content.empty() && content.back() != '\n') out << '\n';
        out << entry << '\n';
    }
};

} // namespace cc::core
