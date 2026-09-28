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
// Structured user settings (RFC-0001 B followup c13b)
// ============================================================

/// Options for ConfigManager::load(). `quiet` performs the identical tier
/// merge but suppresses the §A soft-tier stderr diagnostic; the agent
/// runtime backend is the only quiet caller (a per-call manager inside the
/// TUI must never paint a warning). It is an OPTION on load(), not a
/// parallel routine, so tier logic can never drift.
struct LoadOptions {
    bool quiet = false;
};

/// Value kinds understood by the closed user-setting spec set.
enum class UserSettingKind : std::uint8_t {
    String,
    Boolean,
    UInteger,
    Number,
    Enumeration,
};

/// One projected configuration key. The writable set is CLOSED at seven
/// server-direct-query scalars; nine further keys are projected read-only.
/// `enum_values`/`enum_count` carry the exact-match value list for
/// Enumeration specs (the others leave them empty).
struct UserSettingSpec {
    std::string_view key;       // Canonical dotted token, e.g. "model.temperature".
    std::string_view section;   // Top-level JSON section.
    std::string_view leaf;      // Leaf name inside the section.
    UserSettingKind kind;
    bool writable;
    bool positive;              // UInteger: reject 0 when true.
    std::string_view type;      // JSON type token: string|boolean|integer|number.
    std::string_view env_var;   // Env override name, or empty.
    std::array<std::string_view, 4> enum_values{};
    std::uint8_t enum_count = 0;
};

/// A recognized key that is never writable through the config tool; the
/// guidance names the surface that owns it.
struct BlockedSetting {
    std::string_view key;
    std::string_view guidance;
};

/// Result of a validated user-tier setting write.
struct UserSettingSetOutcome {
    std::filesystem::path path;                 // The user file that was patched.
    std::optional<std::string> repaired;        // null | "trailing_junk_dropped"
                                                // | "replaced_unparseable".
    bool shadowed = false;                      // An env override is engaged.
    std::string shadowed_by;                    // Engaged env var name.
    std::optional<std::string> reload_warning;  // Post-write quiet reload failed.
    std::string value_token;                    // Canonical JSON token written.
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

    // c13b provenance: (section, leaf) scalar leaves seen on disk during the
    // tier merge, and whether the two modeled env overrides are engaged
    // after apply_environment_variables(). Reset on every load().
    std::set<std::pair<std::string, std::string>> disk_leaves_;
    bool env_model_engaged_ = false;
    bool env_max_tokens_engaged_ = false;

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

    /// Load configuration from all sources, merging by priority. Passing
    /// LoadOptions{.quiet = true} suppresses ONLY the §A soft-tier stderr
    /// diagnostics; every tier is read and merged identically.
    [[nodiscard]] Result<void> load(std::optional<LoadOptions> options = std::nullopt) {
        const bool quiet = options.has_value() && options->quiet;
        // Start with defaults
        settings_ = Settings{};

        // Reset the per-tier MCP bookkeeping used by overlay merge and §B.
        mcp_owner_.clear();
        for (auto& names : mcp_physical_names_) names.clear();
        for (auto& list : mcp_file_servers_) list.clear();
        mcp_tier_unparseable_.fill(false);
        mcp_bookkeeping_loaded_ = true;

        // c13b: reset provenance for this fresh merge.
        disk_leaves_.clear();
        env_model_engaged_ = false;
        env_max_tokens_engaged_ = false;

        // RFC-0001 B followup c6: four physical tiers, lowest to highest.
        // global/project parse errors stay HARD failures (those files are
        // only ever written as JSON by Loom); user/local parse errors are
        // tolerated per §A.
        if (auto result = load_tier(global_path_, McpStorageScope::Global, false, quiet);
            !result && result.error().code != ErrorCode::ConfigNotFound) {
            return std::unexpected(result.error());
        }
        if (!user_path_.empty()) {
            if (auto result = load_tier(user_path_, McpStorageScope::User, true, quiet);
                !result && result.error().code != ErrorCode::ConfigNotFound) {
                return std::unexpected(result.error());
            }
        }
        if (auto result = load_tier(project_path_, McpStorageScope::Project, false, quiet);
            !result && result.error().code != ErrorCode::ConfigNotFound) {
            return std::unexpected(result.error());
        }
        if (auto result = load_tier(local_path_, McpStorageScope::Local, true, quiet);
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
    // Structured user settings (RFC-0001 B followup c13b)
    // ================================================================

    /// The closed projected spec table: 7 writable server-direct-query
    /// scalars plus 9 read-only metadata keys.
    [[nodiscard]] static std::span<const UserSettingSpec> user_setting_specs() noexcept;

    /// Find a projected spec by its canonical dotted token (e.g.
    /// "model.temperature"). Returns nullptr outside the closed 16-key set.
    [[nodiscard]] static const UserSettingSpec*
    find_user_setting(std::string_view dotted) noexcept;

    /// The recognized-but-blocked key table with owner-surface guidance.
    [[nodiscard]] static std::span<const BlockedSetting> blocked_settings() noexcept;

    /// Blocked-key guidance for a dotted token, or nullopt.
    [[nodiscard]] static std::optional<std::string_view>
    blocked_setting_message(std::string_view dotted) noexcept;

    /// Compact JSON describing the writable / read-only / blocked key sets
    /// (action=list payload body).
    [[nodiscard]] static std::string serialize_user_setting_specs_json();

    /// Path of the USER tier file this manager writes settings into.
    [[nodiscard]] const std::filesystem::path& user_config_path() const noexcept {
        return user_path_;
    }

    /// Whether the USER tier file existed but was not a parseable JSON
    /// object at the last load() (§A soft failure). A quiet agent load
    /// exposes the state only through this accessor.
    [[nodiscard]] bool user_tier_unparseable() const noexcept {
        return mcp_tier_unparseable_[scope_index(McpStorageScope::User)];
    }

    /// Compact JSON object of the 16 projected keys grouped by section
    /// (model/display/network/permissions), each carrying
    /// {key,type,writable,consumes,source,env_var?,value?}. Used for the
    /// keyless get (get-all) envelope.
    [[nodiscard]] std::string serialize_agent_settings_json() const;

    /// The single-key projection object for a projected dotted token, or
    /// nullopt when the key is outside the closed set (blocked secrets use
    /// agent_secret_presence_json instead).
    [[nodiscard]] std::optional<std::string>
    agent_setting_value_json(std::string_view dotted) const;

    /// Presence-only projection for network.api_key / network.base_url /
    /// network.proxy: {key,set,source} — credential/endpoint bytes are
    /// never serialized. Returns nullopt for other keys.
    [[nodiscard]] std::optional<std::string>
    agent_secret_presence_json(std::string_view dotted) const;

    /// Validate `value` against the closed WRITABLE spec set and patch the
    /// USER tier file (owner-writable, salvage=ON), then quietly reload so
    /// the manager matches a fresh process view. Read-only/blocked/unknown
    /// keys return the terminal D1 errors; a hard failure of the
    /// post-write reload still returns success with `reload_warning`.
    [[nodiscard]] Result<UserSettingSetOutcome>
    set_user_setting(std::string_view dotted,
                     const cc::utils::json::JsonVal& value);

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
    load_tier(const std::filesystem::path& path, McpStorageScope scope,
              bool soft_tier, bool quiet) {
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
                // path loads twice (initial + post-mutation reload). The
                // quiet agent backend neither prints nor marks the path as
                // warned, so a later audible load keeps the warning.
                if (!quiet && mcp_warned_paths_.insert(path.string()).second) {
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
        // c13b: every successfully-applied projected leaf is recorded so
        // source classification can distinguish a file value from a value
        // that merely equals the built-in default.
        auto seen = [this](std::string_view section, std::string_view leaf) {
            disk_leaves_.emplace(std::string(section), std::string(leaf));
        };
        // Model settings
        if (auto model = root.get("model"); model.is_obj()) {
            if (auto v = model.get("default_model"); v.is_str()) {
                settings_.model.default_model = std::string(v.as_str());
                seen("model", "default_model");
            }
            if (auto v = model.get("max_output_tokens"); v.is_num()) {
                settings_.model.max_output_tokens = static_cast<std::uint32_t>(v.as_int());
                seen("model", "max_output_tokens");
            }
            if (auto v = model.get("temperature"); v.is_num()) {
                settings_.model.temperature = v.as_double();
                seen("model", "temperature");
            }
            if (auto v = model.get("extended_thinking"); v.is_bool()) {
                settings_.model.extended_thinking = v.as_bool();
                seen("model", "extended_thinking");
            }
            if (auto v = model.get("thinking_budget"); v.is_num()) {
                settings_.model.thinking_budget = static_cast<std::uint32_t>(v.as_int());
                seen("model", "thinking_budget");
            }
            if (auto v = model.get("context_window_size"); v.is_num()) {
                settings_.model.context_window_size = static_cast<std::uint32_t>(v.as_int());
                seen("model", "context_window_size");
            }
        }

        // Display settings
        if (auto display = root.get("display"); display.is_obj()) {
            if (auto v = display.get("show_thinking"); v.is_bool()) {
                settings_.display.show_thinking = v.as_bool();
                seen("display", "show_thinking");
            }
            if (auto v = display.get("show_token_usage"); v.is_bool()) {
                settings_.display.show_token_usage = v.as_bool();
                seen("display", "show_token_usage");
            }
            if (auto v = display.get("compact_mode"); v.is_bool()) {
                settings_.display.compact_mode = v.as_bool();
                seen("display", "compact_mode");
            }
            if (auto v = display.get("line_width"); v.is_num()) {
                settings_.display.line_width = static_cast<std::uint32_t>(v.as_int());
                seen("display", "line_width");
            }
            if (auto v = display.get("theme"); v.is_str()) {
                settings_.display.theme = std::string(v.as_str());
                seen("display", "theme");
            }
        }

        // Network settings
        if (auto network = root.get("network"); network.is_obj()) {
            if (auto v = network.get("api_key"); v.is_str()) {
                settings_.network.api_key = std::string(v.as_str());
                seen("network", "api_key");
            }
            if (auto v = network.get("base_url"); v.is_str()) {
                settings_.network.base_url = std::string(v.as_str());
                seen("network", "base_url");
            }
            if (auto v = network.get("proxy"); v.is_str()) {
                settings_.network.proxy = std::string(v.as_str());
                seen("network", "proxy");
            }
            if (auto v = network.get("timeout_seconds"); v.is_num()) {
                settings_.network.timeout_seconds = static_cast<std::uint32_t>(v.as_int());
                seen("network", "timeout_seconds");
            }
            if (auto v = network.get("max_retries"); v.is_num()) {
                settings_.network.max_retries = static_cast<std::uint32_t>(v.as_int());
                seen("network", "max_retries");
            }
            if (auto v = network.get("verify_ssl"); v.is_bool()) {
                settings_.network.verify_ssl = v.as_bool();
            }
        }

        // Permission settings
        if (auto perms = root.get("permissions"); perms.is_obj()) {
            if (auto v = perms.get("allow_bash"); v.is_bool()) {
                settings_.permissions.allow_bash = v.as_bool();
                seen("permissions", "allow_bash");
            }
            if (auto v = perms.get("allow_file_write"); v.is_bool()) {
                settings_.permissions.allow_file_write = v.as_bool();
                seen("permissions", "allow_file_write");
            }
            if (auto v = perms.get("allow_network"); v.is_bool()) {
                settings_.permissions.allow_network = v.as_bool();
                seen("permissions", "allow_network");
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

        // XAA IdP connection (TS reference: settings.xaaIdp; consumed by
        // /mcp xaa setup|login|show|clear through settings_.xaa_idp).
        // The section did not exist in the C++ tree before followup c12, so
        // the canonical camelCase keys are the only spellings. Members of
        // the wrong type are ignored, as in every other section.
        if (auto xaa = root.get("xaaIdp"); xaa.is_obj()) {
            if (auto v = xaa.get("issuer"); v.is_str()) {
                settings_.xaa_idp.issuer = std::string(v.as_str());
            }
            if (auto v = xaa.get("clientId"); v.is_str()) {
                settings_.xaa_idp.client_id = std::string(v.as_str());
            }
            if (auto v = xaa.get("callbackPort"); v.is_num()) {
                settings_.xaa_idp.callback_port = static_cast<int>(v.as_int());
            }
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
            env_model_engaged_ = true;
        }

        // HTTPS_PROXY / HTTP_PROXY -> network.proxy
        if (auto* val = std::getenv("HTTPS_PROXY")) {
            settings_.network.proxy = val;
        } else if (auto* val2 = std::getenv("HTTP_PROXY")) {
            settings_.network.proxy = val2;
        }

        // LOOM_MAX_TOKENS -> model.max_output_tokens. An unparsable value is
        // ignored (legacy behavior) and therefore does NOT count as an
        // engaged override for provenance/shadow reporting.
        if (auto* val = std::getenv("LOOM_MAX_TOKENS")) {
            try {
                settings_.model.max_output_tokens = static_cast<std::uint32_t>(std::stoul(val));
                env_max_tokens_engaged_ = true;
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

        // XAA IdP connection, last top-level section alongside the other
        // optional trailing scalars. Omitted entirely while all three fields
        // are unset so a default save stays churn-free; /mcp xaa clear relies
        // on this to remove the section on rewrite. This section is outside
        // the §B mcpServers secret-boundary filter.
        const auto& xaa = settings_.xaa_idp;
        if (!xaa.issuer.empty() || !xaa.client_id.empty() ||
            xaa.callback_port.has_value()) {
            json += ",\n  \"xaaIdp\": {\n";
            bool first_field = true;
            if (!xaa.issuer.empty()) {
                json += std::format("    \"issuer\": \"{}\"",
                                    escape_json(xaa.issuer));
                first_field = false;
            }
            if (!xaa.client_id.empty()) {
                if (!first_field) json += ",\n";
                json += std::format("    \"clientId\": \"{}\"",
                                    escape_json(xaa.client_id));
                first_field = false;
            }
            if (xaa.callback_port.has_value()) {
                if (!first_field) json += ",\n";
                json += std::format("    \"callbackPort\": {}",
                                    *xaa.callback_port);
            }
            json += "\n  }";
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

    // ================================================================
    // c13b user-setting private machinery
    // ================================================================

    /// Coerced user value: the canonical JSON token to write plus the
    /// explicit-null-clear marker for the two optional leaves.
    struct CoercedUserValue {
        std::string token;
        bool null_clear = false;
    };

    [[nodiscard]] static std::string_view
    trim_setting_text(std::string_view text) noexcept {
        while (!text.empty() &&
               (text.front() == ' ' || text.front() == '\t' ||
                text.front() == '\r' || text.front() == '\n')) {
            text.remove_prefix(1);
        }
        while (!text.empty() &&
               (text.back() == ' ' || text.back() == '\t' ||
                text.back() == '\r' || text.back() == '\n')) {
            text.remove_suffix(1);
        }
        return text;
    }

    /// Parse a decimal unsigned integer from text: ASCII digits only (a
    /// leading sign is rejected), full consumption, int64 range.
    [[nodiscard]] static Result<std::int64_t>
    parse_uint_token(std::string_view text) {
        if (text.empty() ||
            !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; })) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput, "must be a non-negative integer"));
        }
        std::int64_t value = 0;
        const auto* begin = text.data();
        const auto [ptr, ec] = std::from_chars(begin, begin + text.size(), value);
        if (ec != std::errc{} || ptr != begin + text.size()) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput, "integer value is out of range"));
        }
        return value;
    }

    /// Parse a finite double from text with full consumption.
    [[nodiscard]] static Result<double>
    parse_double_token(std::string_view raw_text) {
        const std::string_view text = trim_setting_text(raw_text);
        if (text.empty()) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput, "must be a number"));
        }
        double value = 0.0;
        const auto* begin = text.data();
        const auto [ptr, ec] = std::from_chars(
            begin, begin + text.size(), value, std::chars_format::general);
        if (ec != std::errc{} || ptr != begin + text.size() ||
            !std::isfinite(value)) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput, "must be a finite number"));
        }
        return value;
    }

    /// Kind-level coercion (D7). Key-specific ranges (temperature [0,1],
    /// thinking_budget >=1024/0-clear) are applied by set_user_setting.
    [[nodiscard]] static Result<CoercedUserValue>
    coerce_user_value(const UserSettingSpec& spec,
                      const cc::utils::json::JsonVal& value) {
        CoercedUserValue out;
        switch (spec.kind) {
        case UserSettingKind::String: {
            if (!value.is_str()) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be a string", spec.key)));
            }
            const std::string_view trimmed = trim_setting_text(value.as_str());
            if (trimmed.empty()) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must not be empty", spec.key)));
            }
            cc::utils::json::JsonMutDoc doc;
            doc.set_root(doc.string(trimmed));
            out.token = doc.to_string();
            return out;
        }
        case UserSettingKind::Boolean: {
            if (value.is_bool()) {
                out.token = value.as_bool() ? "true" : "false";
                return out;
            }
            if (value.is_str()) {
                const std::string_view text = value.as_str();
                if (text == "true" || text == "false") {
                    out.token = std::string(text);
                    return out;
                }
            }
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput,
                std::format("'{}' must be true or false", spec.key)));
        }
        case UserSettingKind::UInteger: {
            std::int64_t parsed = 0;
            if (value.is_int()) {
                parsed = value.as_int();
            } else if (value.is_str()) {
                auto text_result = parse_uint_token(value.as_str());
                if (!text_result) return std::unexpected(text_result.error());
                parsed = *text_result;
            } else {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be a non-negative integer", spec.key)));
            }
            if (parsed < 0) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must not be negative", spec.key)));
            }
            if (static_cast<std::uint64_t>(parsed) >
                static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' is out of range", spec.key)));
            }
            if (spec.positive && parsed == 0) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be at least 1", spec.key)));
            }
            out.token = std::to_string(parsed);
            return out;
        }
        case UserSettingKind::Number: {
            if (value.is_null()) {
                out.token = "null";
                out.null_clear = true;
                return out;
            }
            double parsed = 0.0;
            if (value.is_num()) {
                parsed = value.as_double();
            } else if (value.is_str()) {
                auto text_result = parse_double_token(value.as_str());
                if (!text_result) return std::unexpected(text_result.error());
                parsed = *text_result;
            } else {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be a number or null", spec.key)));
            }
            cc::utils::json::JsonMutDoc doc;
            doc.set_root(doc.number(parsed));
            out.token = doc.to_string();
            return out;
        }
        case UserSettingKind::Enumeration: {
            if (!value.is_str()) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be a string", spec.key)));
            }
            const std::string_view text = value.as_str();
            bool matched = false;
            for (std::uint8_t i = 0; i < spec.enum_count; ++i) {
                if (spec.enum_values[i] == text) { matched = true; break; }
            }
            if (!matched) {
                std::string allowed;
                for (std::uint8_t i = 0; i < spec.enum_count; ++i) {
                    if (i) allowed += ", ";
                    allowed += std::string(spec.enum_values[i]);
                }
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    std::format("'{}' must be one of: {}", spec.key, allowed)));
            }
            cc::utils::json::JsonMutDoc doc;
            doc.set_root(doc.string(text));
            out.token = doc.to_string();
            return out;
        }
        }
        return std::unexpected(Error::make(
            ErrorCode::InternalError, "unhandled setting kind"));
    }

    /// thinking_budget-specific coercion: null or 0 clears, otherwise a
    /// uint >= 1024 (the Anthropic minimum).
    [[nodiscard]] static Result<CoercedUserValue>
    coerce_thinking_budget(const cc::utils::json::JsonVal& value) {
        constexpr std::string_view key = "model.thinking_budget";
        if (value.is_null()) {
            return CoercedUserValue{"null", true};
        }
        std::int64_t parsed = 0;
        if (value.is_int()) {
            parsed = value.as_int();
        } else if (value.is_str()) {
            const std::string_view text = trim_setting_text(value.as_str());
            if (text == "0") return CoercedUserValue{"null", true};
            auto text_result = parse_uint_token(text);
            if (!text_result) return std::unexpected(text_result.error());
            parsed = *text_result;
        } else {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput,
                std::format("'{}' must be an integer or null", key)));
        }
        if (parsed == 0) return CoercedUserValue{"null", true};
        if (parsed < 1024) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput,
                std::format("'{}' must be at least 1024 (the Anthropic minimum)",
                            key)));
        }
        if (static_cast<std::uint64_t>(parsed) >
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            return std::unexpected(Error::make(
                ErrorCode::InvalidInput,
                std::format("'{}' is out of range", key)));
        }
        return CoercedUserValue{std::to_string(parsed), false};
    }

    /// Effective source token: env override engaged > file leaf seen >
    /// built-in default (precision-note pinned classification).
    [[nodiscard]] std::string_view
    setting_source(const UserSettingSpec& spec) const noexcept {
        if (spec.env_var == "LOOM_MODEL" && env_model_engaged_) return "env";
        if (spec.env_var == "LOOM_MAX_TOKENS" && env_max_tokens_engaged_) return "env";
        if (disk_leaves_.contains(
                std::pair<std::string, std::string>(
                    std::string(spec.section), std::string(spec.leaf)))) {
            return "file";
        }
        return "default";
    }

    /// Append the effective value token for `spec` onto the projection
    /// object (optional leaves emit null when unset).
    void append_effective_value(cc::utils::json::JsonMutVal& token,
                                const UserSettingSpec& spec) const {
        const auto& m = settings_.model;
        const auto& d = settings_.display;
        const auto& n = settings_.network;
        const auto& p = settings_.permissions;
        const std::string_view key = spec.key;
        if (key == "model.default_model") { token.set("value", m.default_model); return; }
        if (key == "model.max_output_tokens") { token.set("value", static_cast<std::int64_t>(m.max_output_tokens)); return; }
        if (key == "model.temperature") {
            if (m.temperature) token.add("value", token.make_real(*m.temperature));
            else token.add("value", token.make_null());
            return;
        }
        if (key == "model.extended_thinking") { token.set("value", m.extended_thinking); return; }
        if (key == "model.thinking_budget") {
            if (m.thinking_budget) token.set("value", static_cast<std::int64_t>(*m.thinking_budget));
            else token.add("value", token.make_null());
            return;
        }
        if (key == "model.context_window_size") { token.set("value", static_cast<std::int64_t>(m.context_window_size)); return; }
        if (key == "display.show_thinking") { token.set("value", d.show_thinking); return; }
        if (key == "display.show_token_usage") { token.set("value", d.show_token_usage); return; }
        if (key == "display.compact_mode") { token.set("value", d.compact_mode); return; }
        if (key == "display.theme") { token.set("value", d.theme); return; }
        if (key == "display.line_width") {
            if (d.line_width) token.set("value", static_cast<std::int64_t>(*d.line_width));
            else token.add("value", token.make_null());
            return;
        }
        if (key == "network.timeout_seconds") { token.set("value", static_cast<std::int64_t>(n.timeout_seconds)); return; }
        if (key == "network.max_retries") { token.set("value", static_cast<std::int64_t>(n.max_retries)); return; }
        if (key == "permissions.allow_bash") { token.set("value", p.allow_bash); return; }
        if (key == "permissions.allow_file_write") { token.set("value", p.allow_file_write); return; }
        if (key == "permissions.allow_network") { token.set("value", p.allow_network); return; }
    }

    /// Build the single-key projection object as compact JSON.
    [[nodiscard]] std::string
    build_setting_token(const UserSettingSpec& spec) const {
        cc::utils::json::JsonMutDoc doc;
        auto token = doc.object();
        token.set("key", spec.key);
        token.set("type", spec.type);
        token.set("writable", spec.writable);
        auto consumes = token.make_arr();
        if (spec.writable) consumes.append(doc.string("server-direct-query"));
        token.add("consumes", consumes);
        if (!spec.env_var.empty()) token.set("env_var", spec.env_var);
        token.set("source", setting_source(spec));
        // The interactive resolver additionally honors these for the model;
        // ConfigManager itself does not model them (D1 source_note).
        if (spec.key == "model.default_model" &&
            (std::getenv("ANTHROPIC_MODEL") != nullptr ||
             std::getenv("ANTHROPIC_DEFAULT_SONNET_MODEL") != nullptr)) {
            token.set("source_note",
                "ANTHROPIC_MODEL/ANTHROPIC_DEFAULT_SONNET_MODEL also override "
                "interactive model resolution");
        }
        append_effective_value(token, spec);
        doc.set_root(token);
        return doc.to_string();
    }

    /// Generic read-modify-write for a JSON OBJECT config file. A missing,
    /// zero-length, or all-whitespace file starts from a fresh `{}`. An
    /// existing file is strict-parsed: an OBJECT root is copied into a
    /// mutable document and `mutate` is applied; anything else (parse
    /// failure or non-object root) is handled per `salvage`:
    ///   - Strict: `strict_error` is returned BEFORE any write, so the file
    ///     bytes are never touched.
    ///   - SalvageTrailing: parse_first keeps a complete leading OBJECT
    ///     (duplicate keys retained exactly as yyjson reads them) and drops
    ///     only trailing bytes; with no recoverable leading OBJECT (pure
    ///     junk, or a leading non-object such as `[1,2]`) the file is
    ///     replaced from `{}`.
    /// `salvage_out` reports how an existing non-blank file's bytes were
    /// reconciled: Untouched (missing/blank, or a strict-valid OBJECT
    /// preserved verbatim), TrailingJunkDropped (a complete leading OBJECT
    /// recovered with parse_first — duplicate keys retained exactly as
    /// yyjson reads them — and only trailing bytes discarded), or
    /// ReplacedUnparseable (no recoverable leading OBJECT — pure junk, or a
    /// leading non-object such as `[1,2]` — so the file is replaced from
    /// `{}`). It is initialized Untouched at entry; Strict mode returns
    /// strict_error before assigning anything else. The SalvageTrailing
    /// mode and its tri-state signal are fully produced here; the first
    /// caller that consumes a salvage result arrives in c13b (the config
    /// tool). On success the result is written atomically via a fixed tmp
    /// name + rename.
    enum class SalvageMode : std::uint8_t { Strict, SalvageTrailing };

    enum class SalvageResult : std::uint8_t {
        Untouched,             // Fresh/blank file or a strict-valid object.
        TrailingJunkDropped,   // Leading OBJECT kept; trailing bytes dropped.
        ReplacedUnparseable,   // No recoverable OBJECT; file rebuilt from {}.
    };

    [[nodiscard]] static VoidResult
    patch_object_file(const std::filesystem::path& path,
                      const std::function<VoidResult(
                          cc::utils::json::JsonMutVal& root,
                          cc::utils::json::JsonMutDoc& doc)>& mutate,
                      bool owner_only,
                      SalvageMode salvage,
                      const Error& strict_error,
                      SalvageResult& salvage_out) {
        cc::utils::json::JsonMutDoc doc;
        cc::utils::json::JsonMutVal root;
        salvage_out = SalvageResult::Untouched;
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
                if (parsed && parsed->root().is_obj()) {
                    root = doc.copy_val(parsed->root());
                    doc.set_root(root);
                } else if (salvage == SalvageMode::Strict) {
                    // Parse failure or non-object root: hand the caller's
                    // pinned error back before any write happens.
                    return std::unexpected(strict_error);
                } else {
                    // SalvageTrailing: preserve a complete leading OBJECT and
                    // drop only the trailing bytes. With no recoverable
                    // OBJECT the whole file is replaced from {}.
                    auto first = cc::utils::json::parse_first(content);
                    if (first && first->root().is_obj()) {
                        root = doc.copy_val(first->root());
                        doc.set_root(root);
                        salvage_out = SalvageResult::TrailingJunkDropped;
                    } else {
                        root = doc.object();
                        doc.set_root(root);
                        salvage_out = SalvageResult::ReplacedUnparseable;
                    }
                }
            }
        }
        if (!file_present || blank) {
            // Missing file or a zero-length/all-whitespace file: start from
            // {} and let the patch create it fresh.
            root = doc.object();
            doc.set_root(root);
        }

        if (auto mutated = mutate(root, doc); !mutated) {
            return std::unexpected(mutated.error());
        }

        if (auto written = write_config_file_atomic(
                path, doc.to_pretty_string(), owner_only);
            !written) {
            return std::unexpected(written.error());
        }
        return {};
    }

    /// Read-modify-write one MCP tier file. Thin wrapper over
    /// patch_object_file: strict parsing with the MCP-specific refusal
    /// text, the mcpServers key ensured inside the mutator and an emptied
    /// object dropped (other top-level sections preserved), LOCAL-tier
    /// owner-only mode, and the local gitignore guard after every
    /// successful write.
    [[nodiscard]] VoidResult
    patch_mcp_file(const std::filesystem::path& path, const McpPatchFn& mutator) {
        // The LOCAL tier is the documented secret holder (it shadows tracked
        // files precisely to keep Authorization headers out of VCS): create
        // it owner-only, like ssh-style secret files. Applied to the tmp
        // BEFORE rename so the final file never briefly exists world-readable.
        const bool target_is_local = (path == local_path_);
        // The exact pre-extraction inline parse-failure error: same code and
        // literal, path interpolated here so the C6-pinned refusal text
        // stays byte-identical.
        const Error strict_error = Error::make(
            ErrorCode::ConfigParseError,
            std::format("{} is not valid JSON; move it aside first "
                        "(use 'loom mcp' to edit MCP configuration)",
                        path.string()));
        // Strict mode always reports Untouched; the tri-state exists for
        // the c13b salvage caller, so discard it here.
        SalvageResult salvage_out = SalvageResult::Untouched;
        auto patched = patch_object_file(path,
            [&](cc::utils::json::JsonMutVal& root,
                cc::utils::json::JsonMutDoc& doc) -> VoidResult {
                auto servers = root.ensure_object("mcpServers");
                if (auto mutated = mutator(servers, doc); !mutated) {
                    return std::unexpected(mutated.error());
                }
                // Drop an emptied mcpServers object; keep other sections untouched.
                if (servers.is_obj() && servers.size() == 0) {
                    (void)root.remove("mcpServers");
                }
                return {};
            },
            /*owner_only=*/target_is_local,
            SalvageMode::Strict,
            strict_error,
            salvage_out);
        if (!patched) return std::unexpected(patched.error());
        (void)salvage_out;
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

// ================================================================
// c13b structured user settings — out-of-class definitions
// ================================================================

[[nodiscard]] inline std::span<const UserSettingSpec>
ConfigManager::user_setting_specs() noexcept {
    // The CLOSED projected set: 7 writable server-direct-query scalars
    // followed by the 9 read-only metadata keys. Order within a section is
    // the serialization order of get-all.
    static constexpr UserSettingSpec specs[] = {
        // ── writable (7) ──
        {"model.default_model",      "model", "default_model",
         UserSettingKind::String,    true, true, "string", "LOOM_MODEL"},
        {"model.max_output_tokens",  "model", "max_output_tokens",
         UserSettingKind::UInteger,  true, true, "integer", "LOOM_MAX_TOKENS"},
        {"model.temperature",        "model", "temperature",
         UserSettingKind::Number,    true, false, "number", ""},
        {"model.extended_thinking",  "model", "extended_thinking",
         UserSettingKind::Boolean,   true, false, "boolean", ""},
        {"model.thinking_budget",    "model", "thinking_budget",
         UserSettingKind::UInteger,  true, true, "integer", ""},
        {"model.context_window_size","model", "context_window_size",
         UserSettingKind::UInteger,  true, true, "integer", ""},
        {"network.max_retries",      "network", "max_retries",
         UserSettingKind::UInteger,  true, false, "integer", ""},
        // ── read-only metadata (9) ──
        {"display.show_thinking",    "display", "show_thinking",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
        {"display.show_token_usage", "display", "show_token_usage",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
        {"display.compact_mode",     "display", "compact_mode",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
        {"display.theme",            "display", "theme",
         UserSettingKind::Enumeration, false, false, "string", "",
         {"auto", "dark", "light"}, 3},
        {"display.line_width",       "display", "line_width",
         UserSettingKind::UInteger,  false, true, "integer", ""},
        {"network.timeout_seconds",  "network", "timeout_seconds",
         UserSettingKind::UInteger,  false, true, "integer", ""},
        {"permissions.allow_bash",       "permissions", "allow_bash",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
        {"permissions.allow_file_write", "permissions", "allow_file_write",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
        {"permissions.allow_network",    "permissions", "allow_network",
         UserSettingKind::Boolean,   false, false, "boolean", ""},
    };
    return specs;
}

[[nodiscard]] inline const UserSettingSpec*
ConfigManager::find_user_setting(std::string_view dotted) noexcept {
    for (const auto& spec : user_setting_specs()) {
        if (spec.key == dotted) return &spec;
    }
    return nullptr;
}

[[nodiscard]] inline std::span<const BlockedSetting>
ConfigManager::blocked_settings() noexcept {
    static constexpr BlockedSetting blocked[] = {
        {"network.api_key",
         "Set the ANTHROPIC_API_KEY environment variable to configure the API "
         "key; this tool never reads or writes credential values."},
        {"network.base_url",
         "Set the ANTHROPIC_BASE_URL environment variable to configure the "
         "endpoint."},
        {"network.proxy",
         "Set the HTTPS_PROXY environment variable (HTTP_PROXY is used as a "
         "fallback) to configure the proxy."},
        {"network.verify_ssl",
         "TLS certificate verification is a security kill switch and cannot "
         "be changed through this tool."},
        {"permissions.deny_rules",
         "Permission deny rules are managed through the mcp and allow "
         "permission surfaces, not this tool."},
        {"permissions.deny",
         "Permission deny rules are managed through the mcp and allow "
         "permission surfaces, not this tool."},
        {"permissions.allowed_paths",
         "Permission path lists are managed through the permission surfaces, "
         "not this tool."},
        {"permissions.denied_paths",
         "Permission path lists are managed through the permission surfaces, "
         "not this tool."},
        {"permissions.allowed_commands",
         "Permission command lists are managed through the permission "
         "surfaces, not this tool."},
        {"systemPrompt",
         "System prompt overrides are session-scoped prompt injection and "
         "cannot be written through this tool."},
        {"customInstructions",
         "Custom instructions are session-scoped and cannot be written "
         "through this tool."},
        {"features",
         "The features bitmask is build/runtime-internal and cannot be "
         "changed through this tool."},
        {"mcpServers",
         "Use the mcp tool or the 'loom mcp' command to manage MCP servers."},
        {"xaaIdp",
         "Use '/mcp xaa' to configure the XAA identity provider."},
    };
    return blocked;
}

[[nodiscard]] inline std::optional<std::string_view>
ConfigManager::blocked_setting_message(std::string_view dotted) noexcept {
    for (const auto& entry : blocked_settings()) {
        if (entry.key == dotted) return entry.guidance;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string
ConfigManager::serialize_user_setting_specs_json() {
    namespace json = cc::utils::json;
    json::JsonMutDoc doc;
    auto root = doc.object();

    auto writable = root.make_arr();
    auto read_only = root.make_arr();
    for (const auto& spec : user_setting_specs()) {
        auto entry = doc.object();
        entry.set("key", spec.key);
        entry.set("type", spec.type);
        entry.set("writable", spec.writable);
        auto consumes = entry.make_arr();
        if (spec.writable) consumes.append(doc.string("server-direct-query"));
        entry.add("consumes", consumes);
        if (spec.kind == UserSettingKind::Enumeration) {
            auto values = entry.make_arr();
            for (std::uint8_t i = 0; i < spec.enum_count; ++i) {
                values.append(doc.string(spec.enum_values[i]));
            }
            entry.add("enum_values", values);
        }
        (spec.writable ? writable : read_only).append(entry);
    }
    root.add("writable", writable);
    root.add("read_only", read_only);

    auto blocked = root.make_arr();
    for (const auto& entry : blocked_settings()) {
        auto item = doc.object();
        item.set("key", entry.key);
        item.set("guidance", entry.guidance);
        blocked.append(item);
    }
    root.add("blocked", blocked);

    auto null_clear = root.make_arr();
    null_clear.append(doc.string("model.temperature"));
    null_clear.append(doc.string("model.thinking_budget"));
    root.add("null_clear_keys", null_clear);
    root.set("null_clear_note",
        "A JSON null clears the leaf in the USER tier file only; a value in "
        "a lower-tier file still wins through the tier merge.");

    doc.set_root(root);
    return doc.to_string();
}

[[nodiscard]] inline std::string
ConfigManager::serialize_agent_settings_json() const {
    namespace json = cc::utils::json;
    json::JsonMutDoc doc;
    auto root = doc.object();
    // Fixed section order; each projected section is always present.
    constexpr std::array<std::string_view, 4> sections = {
        "model", "display", "network", "permissions"};
    for (const auto section_name : sections) {
        auto section = doc.object();
        for (const auto& spec : user_setting_specs()) {
            if (spec.section != section_name) continue;
            section.add(spec.leaf, doc.raw_json(build_setting_token(spec)));
        }
        root.add(section_name, section);
    }
    doc.set_root(root);
    return doc.to_string();
}

[[nodiscard]] inline std::optional<std::string>
ConfigManager::agent_setting_value_json(std::string_view dotted) const {
    const auto* spec = find_user_setting(dotted);
    if (spec == nullptr) return std::nullopt;
    return build_setting_token(*spec);
}

[[nodiscard]] inline std::optional<std::string>
ConfigManager::agent_secret_presence_json(std::string_view dotted) const {
    const char* leaf = nullptr;
    const char* env_value = nullptr;
    if (dotted == "network.api_key") {
        leaf = "api_key";
        env_value = std::getenv("ANTHROPIC_API_KEY");
    } else if (dotted == "network.base_url") {
        leaf = "base_url";
        env_value = std::getenv("ANTHROPIC_BASE_URL");
    } else if (dotted == "network.proxy") {
        leaf = "proxy";
        env_value = std::getenv("HTTPS_PROXY");
        if (env_value == nullptr) env_value = std::getenv("HTTP_PROXY");
    } else {
        return std::nullopt;
    }

    std::string_view source = "none";
    if (env_value != nullptr) {
        source = "env";
    } else if (disk_leaves_.contains(
                   std::pair<std::string, std::string>("network", leaf))) {
        source = "file";
    }

    cc::utils::json::JsonMutDoc doc;
    auto presence = doc.object();
    presence.set("key", dotted);
    presence.set("set", source != "none");
    presence.set("source", source);
    doc.set_root(presence);
    return doc.to_string();
}

[[nodiscard]] inline Result<UserSettingSetOutcome>
ConfigManager::set_user_setting(std::string_view dotted,
                                const cc::utils::json::JsonVal& value) {
    // Classification is against the writable spec set ONLY: blocked keys
    // get owner-surface guidance, projected keys get the terminal
    // not-writable error, everything else is an unknown-key error.
    if (auto guidance = blocked_setting_message(dotted); guidance) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidInput,
            std::format("'{}' cannot be set with this tool. {}",
                        dotted, *guidance)));
    }
    const UserSettingSpec* spec = find_user_setting(dotted);
    if (spec == nullptr) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidInput,
            std::format("Unknown configuration key '{}'. Run action=list to "
                        "see the supported keys.", dotted)));
    }
    if (!spec->writable) {
        return std::unexpected(Error::make(
            ErrorCode::InvalidInput,
            std::format("'{}' is not writable through this tool; no runtime "
                        "component consumes this setting.", dotted)));
    }
    if (user_path_.empty()) {
        return std::unexpected(Error::make(
            ErrorCode::ConfigWriteError,
            "No user configuration file is configured for writes"));
    }

    // Key-specific validation, then kind coercion.
    CoercedUserValue coerced;
    if (dotted == "model.thinking_budget") {
        auto budget = coerce_thinking_budget(value);
        if (!budget) return std::unexpected(budget.error());
        coerced = *budget;
    } else {
        auto generic = coerce_user_value(*spec, value);
        if (!generic) return std::unexpected(generic.error());
        coerced = *generic;
        if (dotted == "model.temperature" && !coerced.null_clear) {
            auto reparsed = cc::utils::json::parse(coerced.token);
            const double temperature =
                reparsed ? reparsed->root().as_double() : 0.0;
            if (temperature < 0.0 || temperature > 1.0) {
                return std::unexpected(Error::make(
                    ErrorCode::InvalidInput,
                    "model.temperature must be between 0 and 1 (or null to "
                    "clear the user-tier value)"));
            }
        }
    }

    UserSettingSetOutcome outcome;
    outcome.path = user_path_;
    outcome.value_token = coerced.token;

    // Env-shadow disclosure (D2): computed straight from the environment
    // rather than this instance's load state.
    if (spec->env_var == "LOOM_MODEL" && std::getenv("LOOM_MODEL") != nullptr) {
        outcome.shadowed = true;
        outcome.shadowed_by = "LOOM_MODEL";
    } else if (spec->env_var == "LOOM_MAX_TOKENS") {
        if (const char* raw = std::getenv("LOOM_MAX_TOKENS"); raw != nullptr) {
            try {
                if (const auto parsed = std::stoul(raw);
                    parsed <= std::numeric_limits<std::uint32_t>::max()) {
                    outcome.shadowed = true;
                    outcome.shadowed_by = "LOOM_MAX_TOKENS";
                }
            } catch (...) {
                // Unparsable env does not engage the override.
            }
        }
    }

    const std::string section(spec->section);
    const std::string leaf(spec->leaf);
    const std::string token = coerced.token;
    // SalvageTrailing never returns this error (it is the strict-mode
    // channel), but the generic patcher requires the caller's pinned
    // literal; this is the config-tool wording.
    const Error strict_error = Error::make(
        ErrorCode::ConfigParseError,
        std::format("{} is not valid JSON; repair or move it aside before "
                    "changing configuration settings", user_path_.string()));
    SalvageResult salvage_out = SalvageResult::Untouched;
    auto patched = patch_object_file(
        user_path_,
        [&](cc::utils::json::JsonMutVal& root,
            cc::utils::json::JsonMutDoc& doc) -> VoidResult {
            auto section_obj = root.ensure_object(section);
            // raw_json("null") yields a valid JSON-null value that
            // yyjson_mut_obj_put KEEPS (only a null pointer deletes), so
            // the explicit null-clear is written into the user file.
            auto leaf_value = doc.raw_json(token);
            if (!leaf_value.valid()) {
                return std::unexpected(Error::make(
                    ErrorCode::InternalError,
                    "Internal error: serialized setting is not valid JSON"));
            }
            section_obj.add(leaf, leaf_value);
            return {};
        },
        /*owner_only=*/false,
        SalvageMode::SalvageTrailing,
        strict_error,
        salvage_out);
    if (!patched) return std::unexpected(patched.error());

    switch (salvage_out) {
    case SalvageResult::TrailingJunkDropped:
        outcome.repaired = "trailing_junk_dropped";
        break;
    case SalvageResult::ReplacedUnparseable:
        outcome.repaired = "replaced_unparseable";
        break;
    case SalvageResult::Untouched:
        break;
    }

    // D4: recompute against the post-rename file with a QUIET reload. A
    // hard failure of another tier still returns success; the write is
    // never rolled back.
    if (auto reloaded = load(LoadOptions{.quiet = true}); !reloaded) {
        outcome.reload_warning = reloaded.error().message;
    }
    return outcome;
}

} // namespace cc::core
