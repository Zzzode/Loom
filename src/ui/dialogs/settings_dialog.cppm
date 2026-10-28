/// @file settings_dialog.cppm
/// @brief Settings interface with tabbed navigation (General, Model, API,
/// Permissions, Tools, MCP, LSP, Bridge, Hooks, Privacy, About, Status, Usage).
/// Config read/write is delegated 100% to ConfigManager (loom.config.config)
/// — no direct JSON I/O.
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#include <cstdint>
#include <ctime>

export module loom.ui.dialogs.settings_dialog;

import loom.ui.foundation.theme_provider;

import std;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.design_tokens;

import loom.types.types;
import loom.config.config;
import loom.ui.widgets.custom_select;

export namespace loom::ui::dialogs::settings_dialog {
using namespace ftxui;

// Check whether `e` is a Ctrl+<letter> combination.
// Control characters in ASCII are 1..26 = letter - 'a' + 1 (case insensitive).
inline bool is_ctrl_key(const Event& e, char letter) {
    char code = (letter >= 'a' && letter <= 'z')   ? (letter - 'a' + 1)
                : (letter >= 'A' && letter <= 'Z') ? (letter - 'A' + 1)
                                                   : 0;
    return code != 0 && e.input().size() == 1 &&
           static_cast<unsigned char>(e.input()[0]) == static_cast<unsigned char>(code);
}

using custom_select::SelectOption;
using loom::core::ConfigManager;
using loom::core::FeatureFlag;
using loom::core::McpServerConfig;
using loom::core::Settings;

// ============================================================
// Types
// ============================================================

/// Top-level tabs visible in the left sidebar
enum class SettingsTabId : std::uint8_t {
    General = 0,
    Model,
    API,
    Permissions,
    Tools,
    MCP,
    LSP,
    Bridge,
    Hooks,
    Privacy,
    About,
    Status,
    Usage,
    _COUNT,
};

/// Convert a tab enum to its sidebar label
[[nodiscard]] inline std::string tab_label(SettingsTabId id) {
    switch (id) {
    case SettingsTabId::General:
        return "General";
    case SettingsTabId::Model:
        return "Model";
    case SettingsTabId::API:
        return "API";
    case SettingsTabId::Permissions:
        return "Permissions";
    case SettingsTabId::Tools:
        return "Tools";
    case SettingsTabId::MCP:
        return "MCP";
    case SettingsTabId::LSP:
        return "LSP";
    case SettingsTabId::Bridge:
        return "Bridge";
    case SettingsTabId::Hooks:
        return "Hooks";
    case SettingsTabId::Privacy:
        return "Privacy";
    case SettingsTabId::About:
        return "About";
    case SettingsTabId::Status:
        return "Status";
    case SettingsTabId::Usage:
        return "Usage";
    case SettingsTabId::_COUNT:
        return "?";
    }
    return "?";
}

/// Default permission mode used by the Permissions tab
enum class DefaultPermissionMode : std::uint8_t {
    Ask,
    Allow,
    Deny,
};

[[nodiscard]] inline std::string perm_mode_str(DefaultPermissionMode m) {
    switch (m) {
    case DefaultPermissionMode::Ask:
        return "Ask";
    case DefaultPermissionMode::Allow:
        return "Allow";
    case DefaultPermissionMode::Deny:
        return "Deny";
    }
    return "Ask";
}

/// Subsystem health state shown on the Status tab
enum class HealthState : std::uint8_t {
    OK,
    Degraded,
    Down,
};

/// One row in the Status tab health listing
struct SubsystemHealth {
    std::string name;
    HealthState state;
    std::chrono::system_clock::time_point last_check;
    std::string message;
};

/// Command result display mode (kept for on_close signature parity)
enum class CommandResultDisplay {
    normal,
    system,
    skip,
};

/// Toast — transient short message shown at bottom after save
struct ToastState {
    std::string message;
    Color color{};  // Explicit caller color; default inherits the current success token.
    std::optional<loom::ui::design::tokens::Role> semantic_role;
    std::chrono::steady_clock::time_point until;

    [[nodiscard]] bool active() const {
        return !message.empty() && std::chrono::steady_clock::now() < until;
    }
};

/// Per-model usage row for the Usage tab
struct ModelUsageRow {
    std::string model_name;
    std::uint64_t requests = 0;
    std::uint64_t input_tokens = 0;
    std::uint64_t output_tokens = 0;
    double cost_usd = 0.0;
};

/// Aggregated usage snapshot
struct UsageSnapshot {
    double total_cost_usd = 0.0;
    std::uint64_t total_input_tokens = 0;
    std::uint64_t total_output_tokens = 0;
    std::uint64_t total_requests = 0;
    double daily_limit_pct = 0.0;  // 0..1
    double rate_limit_pct = 0.0;   // 0..1
    std::vector<ModelUsageRow> per_model;
};

/// Options / props for the settings dialog
struct SettingsDialogOptions {
    /// Called when the dialog is dismissed
    std::function<void(std::optional<std::string>, CommandResultDisplay)> on_close;
    /// Initial tab
    SettingsTabId initial_tab = SettingsTabId::General;
    /// Pre-loaded status diagnostics (optional — can be empty)
    std::vector<SubsystemHealth> status_rows;
    /// Pre-loaded usage snapshot (optional — can be empty)
    std::function<void(UsageSnapshot&)> refresh_usage;
};

// ============================================================
// Helpers — health / toast
// ============================================================

[[nodiscard]] inline std::pair<std::string, Color> health_display(HealthState s) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (s) {
    case HealthState::OK:
        return {std::string{loom::ui::design::figures::kTick}, pal.success};
    case HealthState::Degraded:
        return {std::string{loom::ui::design::figures::kWarning}, pal.warning};
    case HealthState::Down:
        return {std::string{loom::ui::design::figures::kCross}, pal.danger};
    }
    return {"?", pal.text};
}

[[nodiscard]] inline std::string format_ts(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(tp);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

// ============================================================
// Individual setting row builder
// ============================================================

// Wrap by terminal cells after the dialog receives its allocated box. Keep
// explicit line breaks and full-width glyphs intact, including unbroken URLs.
[[nodiscard]] Element SettingsText(const std::string& value, int columns = 0);

[[nodiscard]] Element FocusedSetting(Element element);

[[nodiscard]] int ValueColumns(int columns);

// Width is already known here. Pack intact controls into deterministic rows
// so their full wrapped height is available before scroll-frame measurement.
[[nodiscard]] Element SettingsFlow(Elements items, int columns, int gap = 2);

// Only Elements are rebuilt during box negotiation. The interactive Component
// and its working settings retain their identity through terminal resizing.
class SettingsLayoutNode : public Node {
public:
    explicit SettingsLayoutNode(std::function<Element(int, int)> render);
    void ComputeRequirement() override;
    void SetBox(Box box) override;
    void Check(Status* status) override;

private:
    std::function<Element(int, int)> render_;
    int columns_ = 80;
    int rows_ = 26;
    int computed_columns_ = 0;
    int computed_rows_ = 0;
};

/// Build a standard setting row: label | control | description.
/// Each row takes label text, a control Element, an optional description
/// (enabled hint text uses readable secondary ink below the row).
[[nodiscard]] inline Element
SettingRow(const std::string& label, Element control,
           const std::optional<std::string>& description = std::nullopt, bool disabled = false,
           bool is_modified = false, int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;

    auto label_el =
        SettingsText(" " + label, columns > 0 ? columns - 1 : 0) | (disabled ? dim : nothing);
    auto mod_mark = is_modified ? text("*") | color(pal.warning) : text(" ");
    Element row;
    if (columns > 0 && columns < 52) {
        row = vbox({hbox({mod_mark, label_el}), hbox({text("  "), std::move(control) | flex})});
    } else {
        row = hbox({mod_mark, label_el | size(WIDTH, EQUAL, 28) | flex_shrink,
                    std::move(control) | flex, text(" ")});
    }
    Elements result{row};
    if (description && !description->empty()) {
        result.push_back(hbox(
            {text("   "), SettingsText(*description, columns > 0 ? std::max(1, columns - 3) : 0) |
                              color(pal.muted) | (disabled ? dim : nothing)}));
    }
    return vbox(result);
}

// ============================================================
// Tab: General
// ============================================================

/// In-memory copy of settings being edited. We read from ConfigManager on
/// construction, mutate this working copy while the user edits, and write
/// back only on Save / Reset.
struct WorkingSettings {
    // General / Display
    std::string theme = "dark";
    bool auto_mode = false;
    std::string effort_level = "medium";
    bool fast_mode = false;
    bool show_thinking = true;
    bool show_token_usage = false;
    bool compact_mode = false;

    // Model
    std::string default_model;   // no default — user configures their model
    std::string teammate_model;  // swarm teammate model override

    // API
    std::optional<std::string> base_url;
    std::optional<std::string> api_key;
    std::uint32_t timeout_seconds = 120;
    bool verify_ssl = true;
    std::optional<std::string> proxy;
    std::uint32_t max_retries = 3;

    // Permissions
    DefaultPermissionMode default_perm = DefaultPermissionMode::Ask;
    bool allow_bash = true;
    bool allow_file_write = true;
    bool allow_network = true;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> denied_paths;
    std::vector<std::string> allowed_commands;

    // Tools
    bool enable_agent_tool = true;
    bool enable_web_fetch = true;
    bool enable_web_search = true;
    bool enable_bash_granular = true;
    bool enable_glob_grep_safety = true;
    bool enable_skill_loading = true;

    // MCP
    std::vector<McpServerConfig> mcp_servers;

    // LSP
    bool enable_lsp = true;
    std::uint32_t lsp_timeout = 60;

    // Bridge
    bool enable_bridge = false;
    bool bridge_outbound_only = false;
    std::uint32_t bridge_port = 37246;
    std::string bridge_jwt_secret;  // JWT secret (display only)
    std::vector<std::string> bridge_allowed_origins;
};

/// Copy effective values from ConfigManager into a working set
[[nodiscard]] inline WorkingSettings snapshot_from(const ConfigManager& cfg) {
    WorkingSettings w;
    const auto& s = cfg.settings();
    w.theme = s.display.theme.empty() ? std::string("auto") : s.display.theme;
    w.show_thinking = s.display.show_thinking;
    w.show_token_usage = s.display.show_token_usage;
    w.compact_mode = s.display.compact_mode;
    w.default_model = s.model.default_model;
    w.base_url = s.network.base_url;
    w.api_key = s.network.api_key;
    w.timeout_seconds = s.network.timeout_seconds;
    w.verify_ssl = s.network.verify_ssl;
    w.proxy = s.network.proxy;
    w.max_retries = s.network.max_retries;
    w.allow_bash = s.permissions.allow_bash;
    w.allow_file_write = s.permissions.allow_file_write;
    w.allow_network = s.permissions.allow_network;
    w.allowed_paths = s.permissions.allowed_paths;
    w.denied_paths = s.permissions.denied_paths;
    w.allowed_commands = s.permissions.allowed_commands;
    w.mcp_servers = s.mcp_servers;
    w.enable_skill_loading = cfg.is_feature_enabled(FeatureFlag::SkillSystem);
    return w;
}

/// Write working copy back to ConfigManager
inline void apply_to(const WorkingSettings& w, ConfigManager& cfg) {
    auto& s = cfg.settings_mut();
    s.display.theme = w.theme;
    s.display.show_thinking = w.show_thinking;
    s.display.show_token_usage = w.show_token_usage;
    s.display.compact_mode = w.compact_mode;
    s.model.default_model = w.default_model;
    s.network.base_url = w.base_url;
    s.network.api_key = w.api_key;
    s.network.timeout_seconds = w.timeout_seconds;
    s.network.verify_ssl = w.verify_ssl;
    s.network.proxy = w.proxy;
    s.network.max_retries = w.max_retries;
    s.permissions.allow_bash = w.allow_bash;
    s.permissions.allow_file_write = w.allow_file_write;
    s.permissions.allow_network = w.allow_network;
    s.permissions.allowed_paths = w.allowed_paths;
    s.permissions.denied_paths = w.denied_paths;
    s.permissions.allowed_commands = w.allowed_commands;
    s.mcp_servers = w.mcp_servers;
    cfg.set_feature(FeatureFlag::SkillSystem, w.enable_skill_loading);
}

// ============================================================
// Rendering: tab contents
// ============================================================

[[nodiscard]] inline Element RenderTabHeader(const std::string& title,
                                             const std::string& subtitle = {}, int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements els;
    els.push_back(hbox({
        SettingsText(" " + title + " ", columns) | bold | color(pal.info),
        filler(),
    }));
    if (!subtitle.empty()) {
        els.push_back(hbox({
            text(" "),
            SettingsText(subtitle, columns > 0 ? columns - 1 : 0) | color(pal.muted),
        }));
    }
    els.push_back(separator());
    return vbox(els);
}

// --- General tab ---
[[nodiscard]] inline Element RenderGeneralTab(const WorkingSettings& w, int focus_row,
                                              int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader("General", "Theme, behaviour & defaults", columns));

    // Theme dropdown display
    body.push_back(SettingRow("Theme",
                              SettingsText(" [" + w.theme + "] ", ValueColumns(columns)) |
                                  color(pal.warning) | (focus_row == 0 ? FocusedSetting : nothing),
                              "UI color scheme (dark / light / high-contrast / auto)", false,
                              focus_row == -2, columns));

    // Auto mode
    body.push_back(
        SettingRow("Auto mode",
                   SettingsText(w.auto_mode ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.auto_mode ? pal.success : pal.muted) |
                       (focus_row == 1 ? FocusedSetting : nothing),
                   "Automatically continue tool-use chains without confirmations", false,
                   focus_row == -2, columns));

    // Effort level
    body.push_back(SettingRow("Effort level",
                              SettingsText(" [" + w.effort_level + "] ", ValueColumns(columns)) |
                                  color(pal.warning) | (focus_row == 2 ? FocusedSetting : nothing),
                              "Reasoning effort: low / medium / high", false, focus_row == -2,
                              columns));

    // Fast mode
    body.push_back(SettingRow(
        "Fast mode",
        SettingsText(w.fast_mode ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.fast_mode ? pal.success : pal.muted) |
            (focus_row == 3 ? FocusedSetting : nothing),
        "Use the faster (cheaper) model when available", false, focus_row == -2, columns));

    body.push_back(separator());
    body.push_back(SettingsText("  Display options:", ValueColumns(columns)) | color(pal.muted));
    body.push_back(
        SettingRow("Show thinking",
                   SettingsText(w.show_thinking ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.show_thinking ? pal.success : pal.muted) |
                       (focus_row == 4 ? FocusedSetting : nothing),
                   "Display thinking blocks in assistant output", false, false, columns));
    body.push_back(
        SettingRow("Token usage display",
                   SettingsText(w.show_token_usage ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.show_token_usage ? pal.success : pal.muted) |
                       (focus_row == 5 ? FocusedSetting : nothing),
                   "Show token counters after each response", false, false, columns));
    body.push_back(
        SettingRow("Compact mode",
                   SettingsText(w.compact_mode ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.compact_mode ? pal.success : pal.muted) |
                       (focus_row == 6 ? FocusedSetting : nothing),
                   "Minimal output formatting for smaller terminals", false, false, columns));

    return vbox(body);
}

// --- Model tab ---
[[nodiscard]] inline Element RenderModelTab(const WorkingSettings& w, int focus_row,
                                            int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader("Model", "Default and teammate models", columns));

    body.push_back(SettingRow("Default model",
                              SettingsText(" [" + w.default_model + "] ▾ ", ValueColumns(columns)) |
                                  color(pal.info) | (focus_row == 0 ? FocusedSetting : nothing),
                              "Model used for new sessions", false, false, columns));

    body.push_back(
        SettingRow("Teammate model",
                   SettingsText(w.teammate_model.empty() ? std::string(" (same as default) ")
                                                         : " [" + w.teammate_model + "] ",
                                ValueColumns(columns)) |
                       color(pal.warning) | (focus_row == 1 ? FocusedSetting : nothing),
                   "Override model for sub-agent / swarm teammates", false, false, columns));
    return vbox(body);
}

// --- API tab ---
[[nodiscard]] inline Element RenderAPITab(const WorkingSettings& w, int focus_row,
                                          int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader("API", "Endpoints, authentication and timeouts", columns));

    // Base URL
    std::string url_display = w.base_url.value_or("(no default — set LOOM_BASE_URL)");
    body.push_back(SettingRow("Base URL",
                              SettingsText(" " + url_display + " ", ValueColumns(columns)) |
                                  color(pal.info) | (focus_row == 0 ? FocusedSetting : nothing),
                              "Override the API endpoint (for proxies, self-hosted, etc.)", false,
                              false, columns));

    // API key — masked as ***
    std::string key_display;
    if (w.api_key && !w.api_key->empty()) {
        auto k = *w.api_key;
        if (k.size() <= 8)
            key_display = std::string(k.size(), '*');
        else
            key_display = k.substr(0, 4) + "***" + k.substr(k.size() - 3);
    } else {
        key_display = "(not set — will read $LOOM_API_KEY)";
    }
    body.push_back(SettingRow("API Key",
                              SettingsText(" " + key_display + " ", ValueColumns(columns)) |
                                  color(pal.warning) | (focus_row == 1 ? FocusedSetting : nothing),
                              "Stored in user config. Leave blank to use the environment variable",
                              false, false, columns));

    body.push_back(
        SettingRow("Timeout (s)",
                   SettingsText(std::format(" {} ", w.timeout_seconds), ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 2 ? FocusedSetting : nothing),
                   "Per-request timeout in seconds", false, false, columns));

    body.push_back(SettingRow(
        "Verify SSL",
        SettingsText(w.verify_ssl ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.verify_ssl ? pal.success : pal.danger) |
            (focus_row == 3 ? FocusedSetting : nothing),
        "Verify server TLS certificates (disable only for dev proxies)", false, false, columns));

    body.push_back(separator());
    body.push_back(SettingRow("HTTP proxy",
                              SettingsText(w.proxy.value_or("(none)"), ValueColumns(columns)) |
                                  color(pal.info) | (focus_row == 4 ? FocusedSetting : nothing),
                              "HTTPS_PROXY / HTTP_PROXY override (env takes precedence)", false,
                              false, columns));
    body.push_back(
        SettingRow("Max retries",
                   SettingsText(std::format(" {} ", w.max_retries), ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 5 ? FocusedSetting : nothing),
                   "Number of retry attempts per failed request", false, false, columns));
    return vbox(body);
}

// --- Permissions tab ---
[[nodiscard]] inline Element RenderPermissionsTab(const WorkingSettings& w, int focus_row,
                                                  int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(
        RenderTabHeader("Permissions", "Default behaviour when tools request access", columns));

    body.push_back(SettingRow(
        "Default mode",
        SettingsText(" [" + perm_mode_str(w.default_perm) + "] ", ValueColumns(columns)) |
            color(pal.warning) | (focus_row == 0 ? FocusedSetting : nothing),
        "Ask for confirmation, always allow, or always deny", false, false, columns));

    body.push_back(
        SettingRow("Allow Bash",
                   SettingsText(w.allow_bash ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.allow_bash ? pal.success : pal.muted) |
                       (focus_row == 1 ? FocusedSetting : nothing),
                   "Permit shell command execution", false, false, columns));

    body.push_back(
        SettingRow("Allow file write",
                   SettingsText(w.allow_file_write ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.allow_file_write ? pal.success : pal.muted) |
                       (focus_row == 2 ? FocusedSetting : nothing),
                   "Permit file creation and modification", false, false, columns));

    body.push_back(
        SettingRow("Allow network",
                   SettingsText(w.allow_network ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.allow_network ? pal.success : pal.muted) |
                       (focus_row == 3 ? FocusedSetting : nothing),
                   "Permit outbound HTTP requests (web fetch / search)", false, false, columns));

    body.push_back(separator());
    body.push_back(
        SettingRow("Allowed paths",
                   SettingsText(w.allowed_paths.empty()
                                    ? std::string(" (all) ")
                                    : std::format(" [{} entries] ", w.allowed_paths.size()),
                                ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 4 ? FocusedSetting : nothing),
                   "Whitelisted file paths for tool access", false, false, columns));
    body.push_back(SettingRow(
        "Denied paths",
        SettingsText(w.denied_paths.empty() ? std::string(" (none) ")
                                            : std::format(" [{} entries] ", w.denied_paths.size()),
                     ValueColumns(columns)) |
            color(pal.info) | (focus_row == 5 ? FocusedSetting : nothing),
        "Blacklisted file paths for tool access", false, false, columns));
    body.push_back(
        SettingRow("Allowed commands",
                   SettingsText(w.allowed_commands.empty()
                                    ? std::string(" (all) ")
                                    : std::format(" [{} entries] ", w.allowed_commands.size()),
                                ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 6 ? FocusedSetting : nothing),
                   "Whitelisted shell commands", false, false, columns));
    return vbox(body);
}

// --- Tools tab ---
[[nodiscard]] inline Element RenderToolsTab(const WorkingSettings& w, int focus_row,
                                            int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader("Tools", "Enable or disable top-level tools", columns));

    body.push_back(SettingRow(
        "Agent / Sub-agent",
        SettingsText(w.enable_agent_tool ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.enable_agent_tool ? pal.success : pal.muted) |
            (focus_row == 0 ? FocusedSetting : nothing),
        "Allow the assistant to spawn sub-agents for parallel work", false, false, columns));
    body.push_back(
        SettingRow("Web fetch",
                   SettingsText(w.enable_web_fetch ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.enable_web_fetch ? pal.success : pal.muted) |
                       (focus_row == 1 ? FocusedSetting : nothing),
                   "Fetch arbitrary URLs and return their content", false, false, columns));
    body.push_back(SettingRow(
        "Web search",
        SettingsText(w.enable_web_search ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.enable_web_search ? pal.success : pal.muted) |
            (focus_row == 2 ? FocusedSetting : nothing),
        "Run web searches via the configured provider", false, false, columns));

    body.push_back(separator());
    body.push_back(SettingRow(
        "Bash granular controls",
        SettingsText(w.enable_bash_granular ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.enable_bash_granular ? pal.success : pal.muted) |
            (focus_row == 3 ? FocusedSetting : nothing),
        "Fine-grained per-command permission toggles", false, false, columns));
    body.push_back(SettingRow(
        "Glob / Grep safety",
        SettingsText(w.enable_glob_grep_safety ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.enable_glob_grep_safety ? pal.success : pal.muted) |
            (focus_row == 4 ? FocusedSetting : nothing),
        "Restrict search tools to allowed paths only", false, false, columns));
    body.push_back(SettingRow(
        "Skill loading",
        SettingsText(w.enable_skill_loading ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.enable_skill_loading ? pal.success : pal.muted) |
            (focus_row == 5 ? FocusedSetting : nothing),
        "Enable the skill system for extensible agent capabilities", false, false, columns));
    return vbox(body);
}

// --- MCP tab ---
[[nodiscard]] inline Element RenderMCPTab(const WorkingSettings& w, int focus_row,
                                          int selected_server, int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader(
        "MCP", "Model Context Protocol servers (managed by services/mcp)", columns));

    body.push_back(SettingRow("Server lifecycle",
                              SettingsText(" [managed by runtime] ", ValueColumns(columns)) |
                                  color(pal.info) | (focus_row == 0 ? FocusedSetting : nothing),
                              "Use /mcp to configure servers and inspect runtime status", false,
                              false, columns));

    // Server list
    body.push_back(SettingsText("", ValueColumns(columns)));
    body.push_back(SettingsText(" Configured servers:", ValueColumns(columns)) | bold);
    body.push_back(separator());
    if (w.mcp_servers.empty()) {
        body.push_back(
            SettingsText("   (none — use /mcp add to configure)", ValueColumns(columns)) |
            color(pal.muted));
    } else {
        for (std::size_t i = 0; i < w.mcp_servers.size(); ++i) {
            const auto& pal = *loom::ui::design::theme::current_theme().palette;
            const auto& s = w.mcp_servers[i];
            bool sel = (int)i == selected_server;
            auto cmd = s.command.empty() ? s.url.value_or("(remote)") : s.command;
            auto row = vbox({
                SettingsText(std::string(sel ? "› " : "  ") + s.name, columns) |
                    (sel ? bold : nothing),
                SettingsText("  [" + s.transport + "]", columns) | color(pal.muted),
                SettingsText("  " + cmd, columns) | color(pal.info),
            });
            if (sel) row = row | focus;
            if (sel) {
                const auto& theme = loom::ui::design::theme::current_theme();
                // Monochrome info and surface_tint_2 share the white index.
                const auto selected_bg =
                    theme.variant == loom::ui::design::theme::ThemeVariant::Monochrome
                        ? pal.message_actions_background
                        : pal.surface_tint_2;
                row = row | bgcolor(selected_bg);
            }
            body.push_back(row);
        }
    }

    body.push_back(SettingsText("", ValueColumns(columns)));
    body.push_back(SettingsText(" [a] /mcp add   [r] Remove selected", columns) | color(pal.muted));

    body.push_back(separator());
    body.push_back(SettingsText(" Servers connect and restart through the MCP runtime", columns) |
                   color(pal.muted));
    return vbox(body);
}

// --- LSP tab ---
[[nodiscard]] inline Element RenderLSPTab(const WorkingSettings& w, int focus_row,
                                          int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(RenderTabHeader("LSP", "Language Server Protocol clients", columns));

    body.push_back(
        SettingRow("Enable LSP",
                   SettingsText(w.enable_lsp ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.enable_lsp ? pal.success : pal.muted) |
                       (focus_row == 0 ? FocusedSetting : nothing),
                   "Start LSP clients for detected project languages", false, false, columns));
    body.push_back(
        SettingRow("LSP timeout (s)",
                   SettingsText(std::format(" {} ", w.lsp_timeout), ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 1 ? FocusedSetting : nothing),
                   "Per-request timeout for LSP operations", false, false, columns));
    body.push_back(SettingRow(
        "LSP instances", SettingsText(" (delegates to services/lsp) ", ValueColumns(columns)) | dim,
        "Running instances shown on the Status tab", false, false, columns));

    body.push_back(separator());
    body.push_back(SettingRow(
        "Per-language LSP config",
        SettingsText(" (delegates to services/lsp registry) ", ValueColumns(columns)) |
            color(pal.info),
        "Language-specific LSP server configurations managed externally", false, false, columns));
    return vbox(body);
}

// --- Bridge tab ---
[[nodiscard]] inline Element RenderBridgeTab(const WorkingSettings& w, int focus_row,
                                             int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(
        RenderTabHeader("Bridge", "IDE / editor bridge (VS Code, JetBrains, etc.)", columns));

    body.push_back(
        SettingRow("Enable bridge",
                   SettingsText(w.enable_bridge ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
                       color(w.enable_bridge ? pal.success : pal.muted) |
                       (focus_row == 0 ? FocusedSetting : nothing),
                   "Listen for incoming editor connections", false, false, columns));
    body.push_back(SettingRow(
        "Outbound-only mode",
        SettingsText(w.bridge_outbound_only ? " [ON]  " : " [OFF] ", ValueColumns(columns)) |
            color(w.bridge_outbound_only ? pal.success : pal.muted) |
            (focus_row == 1 ? FocusedSetting : nothing),
        "Only allow REPL→editor calls (security hardening)", false, false, columns));
    body.push_back(
        SettingRow("Listen port",
                   SettingsText(std::format(" {} ", w.bridge_port), ValueColumns(columns)) |
                       color(pal.info) | (focus_row == 2 ? FocusedSetting : nothing),
                   "TCP port for the bridge JWT-authenticated server", false, false, columns));

    body.push_back(separator());
    body.push_back(
        SettingRow("JWT secret",
                   SettingsText(w.bridge_jwt_secret.empty() ? std::string(" (auto-generated) ")
                                                            : std::string(" [configured] "),
                                ValueColumns(columns)) |
                       color(pal.warning) | (focus_row == 3 ? FocusedSetting : nothing),
                   "JWT secret for bridge authentication (rotate via CLI)", false, false, columns));
    body.push_back(SettingRow(
        "Allowed origins",
        SettingsText(w.bridge_allowed_origins.empty()
                         ? std::string(" (any) ")
                         : std::format(" [{} entries] ", w.bridge_allowed_origins.size()),
                     ValueColumns(columns)) |
            color(pal.info) | (focus_row == 4 ? FocusedSetting : nothing),
        "CORS origin allowlist for bridge connections", false, false, columns));
    return vbox(body);
}

[[nodiscard]] inline Element RenderHooksTab(int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        RenderTabHeader("Hooks", "Lifecycle hook configuration", columns),
        SettingRow("Configuration",
                   SettingsText(" Managed in settings JSON ", ValueColumns(columns)) |
                       color(pal.info),
                   "Hook matchers and commands are loaded by the native hook registry", false,
                   false, columns),
        SettingRow("Runtime",
                   SettingsText(" PreToolUse / PostToolUse / Stop ", ValueColumns(columns)) |
                       color(pal.success),
                   "Hook execution is reported through the lifecycle hook service", false, false,
                   columns),
    });
}

[[nodiscard]] inline Element RenderPrivacyTab(int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        RenderTabHeader("Privacy", "Telemetry and data handling", columns),
        SettingRow("Telemetry",
                   SettingsText(" Uses configured analytics sink ", ValueColumns(columns)) |
                       color(pal.info),
                   "Respect environment and managed settings opt-out controls", false, false,
                   columns),
        SettingRow("Secrets",
                   SettingsText(" Redacted in UI and logs ", ValueColumns(columns)) |
                       color(pal.success),
                   "API keys and sensitive tokens are never shown as editable sample values", false,
                   false, columns),
    });
}

[[nodiscard]] inline Element RenderAboutTab(int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        RenderTabHeader("About", "Loom native", columns),
        SettingRow("Runtime",
                   SettingsText(" C++23 modules ", ValueColumns(columns)) | color(pal.info),
                   "Native CLI implementation with CMake/CTest gates", false, false, columns),
        SettingRow("Version", SettingsText(" 1.0.0 ", ValueColumns(columns)) | color(pal.warning),
                   "Project version from CMakeLists.txt", false, false, columns),
    });
}

// --- Status tab (health lights) ---
[[nodiscard]] inline Element RenderStatusTab(const std::vector<SubsystemHealth>& rows,
                                             int columns = 0) {
    Elements body;
    body.push_back(
        RenderTabHeader("Status", "Live health of API, MCP, LSP and bridge subsystems", columns));

    if (rows.empty()) {
        body.push_back(
            SettingsText("  (no diagnostics yet — running checks…)", ValueColumns(columns)) |
            color(loom::ui::design::theme::current_theme().palette->muted));
        return vbox(body);
    }

    for (const auto& r : rows) {
        auto [icon, col] = health_display(r.state);
        auto state_label = (r.state == HealthState::OK)         ? "OK"
                           : (r.state == HealthState::Degraded) ? "Degraded"
                                                                : "Down";
        auto line = vbox({
            SettingsText(" " + icon + " " + r.name, columns) | color(col) | bold,
            SettingsText(std::string("   ") + state_label + "  " + format_ts(r.last_check),
                         columns) |
                color(col),
            SettingsText("   " + r.message, columns) |
                color(loom::ui::design::theme::current_theme().palette->muted),
        });
        body.push_back(line);
    }
    return vbox(body);
}

// --- Usage tab (cards + gauges + table) ---
[[nodiscard]] inline Element RenderUsageTab(const UsageSnapshot& u, int columns = 0) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements body;
    body.push_back(
        RenderTabHeader("Usage", "Cost, tokens and request counters (current session)", columns));

    // --- 4 summary cards ---
    auto card = [](const std::string& label, const std::string& value, Color c) {
        return vbox({
                   SettingsText(" " + label, 20) | dim,
                   SettingsText(" " + value, 20) | bold | color(c),
               }) |
               border | size(WIDTH, EQUAL, 22);
    };

    Elements cards = {
        card("Total Cost", std::format("${:.4f}", u.total_cost_usd), pal.success),
        card("Input Tokens", std::format("{}", u.total_input_tokens), pal.info),
        card("Output Tokens", std::format("{}", u.total_output_tokens), pal.info),
        card("Requests", std::format("{}", u.total_requests), pal.warning),
    };
    body.push_back(SettingsFlow(std::move(cards), columns, 1));
    body.push_back(SettingsText("", ValueColumns(columns)));

    // --- Progress bars (gauge) ---
    body.push_back(SettingsText(" Limits", ValueColumns(columns)) | bold);
    body.push_back(separator());

    auto pct_label = [](double p) { return std::format("{}%", static_cast<int>(p * 100)); };

    body.push_back(hbox({
        SettingsText(" Daily limit ", ValueColumns(columns)) | size(WIDTH, EQUAL, 16),
        gauge(std::clamp(u.daily_limit_pct, 0.0, 1.0)) |
            color(u.daily_limit_pct > 0.9 ? pal.danger : pal.success) |
            size(WIDTH, EQUAL, columns > 0 ? std::max(1, columns - 23) : 40),
        SettingsText(" " + pct_label(u.daily_limit_pct) + " ", ValueColumns(columns)) | dim,
    }));
    body.push_back(hbox({
        SettingsText(" Rate limit ", ValueColumns(columns)) | size(WIDTH, EQUAL, 16),
        gauge(std::clamp(u.rate_limit_pct, 0.0, 1.0)) |
            color(u.rate_limit_pct > 0.9 ? pal.danger : pal.warning) |
            size(WIDTH, EQUAL, columns > 0 ? std::max(1, columns - 23) : 40),
        SettingsText(" " + pct_label(u.rate_limit_pct) + " ", ValueColumns(columns)) | dim,
    }));
    body.push_back(SettingsText("", ValueColumns(columns)));

    // --- Per-model table ---
    body.push_back(SettingsText(" Per-model breakdown", ValueColumns(columns)) | bold);
    body.push_back(separator());
    if (u.per_model.empty()) {
        body.push_back(SettingsText("  (no requests recorded yet)", ValueColumns(columns)) | dim);
    } else {
        for (const auto& r : u.per_model) {
            body.push_back(SettingsText(" " + r.model_name, columns) | bold);
            body.push_back(SettingsText(std::format(" Requests: {}", r.requests), columns) |
                           color(pal.warning));
            body.push_back(
                SettingsText(std::format(" Tokens: {}↓ {}↑", r.input_tokens, r.output_tokens),
                             columns) |
                color(pal.info));
            body.push_back(SettingsText(std::format(" Cost: ${:.4f}", r.cost_usd), columns) |
                           color(pal.success));
        }
    }
    body.push_back(SettingsText("", ValueColumns(columns)));
    body.push_back(hbox({
                       SettingsText(" [r]", ValueColumns(columns)) | color(pal.info),
                       SettingsText("efresh  ", ValueColumns(columns)),
                   }) |
                   dim);

    return vbox(body);
}

// ============================================================
// Rendering: dialog shell (sidebar + tab content + footer)
// ============================================================

[[nodiscard]] inline Element RenderSidebar(SettingsTabId selected) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements items;
    items.push_back(text(" Sections") | color(pal.muted));
    items.push_back(separator());

    // Iterate all tabs in the sidebar order
    constexpr std::array<SettingsTabId, 13> kOrder = {{
        SettingsTabId::General,
        SettingsTabId::Model,
        SettingsTabId::API,
        SettingsTabId::Permissions,
        SettingsTabId::Tools,
        SettingsTabId::MCP,
        SettingsTabId::LSP,
        SettingsTabId::Bridge,
        SettingsTabId::Hooks,
        SettingsTabId::Privacy,
        SettingsTabId::About,
        SettingsTabId::Status,
        SettingsTabId::Usage,
    }};

    for (auto id : kOrder) {
        bool sel = (id == selected);
        auto el = text(" " + tab_label(id)) | (sel ? bold : nothing);
        if (sel)
            el = el | inverted;
        else
            el = el | color(pal.muted);
        items.push_back(el);
    }

    return vbox(items) | size(WIDTH, EQUAL, 18) | border;
}

[[nodiscard]] inline Element RenderFooter(const ToastState& toast, bool dirty, int columns = 0,
                                          std::array<Box, 3>* action_boxes = nullptr) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto action = [&](std::string label, Color ink, int index, bool enabled = true) {
        auto element = text(std::move(label)) | color(ink) | bold;
        if (!enabled) element = element | dim;
        if (action_boxes) element = element | reflect((*action_boxes)[index]);
        return element;
    };
    Elements rows{separator(), SettingsFlow(
                                   {
                                       action("Ctrl+S Save", pal.success, 0, dirty),
                                       action("Ctrl+R Reset", pal.warning, 1),
                                       action("Esc Close", pal.danger, 2),
                                   },
                                   columns)};
    if (dirty) rows.push_back(SettingsText("* unsaved changes", columns) | color(pal.warning));
    if (toast.active()) {
        // Long diagnostics belong to the scrollable document. Keep the action
        // bar bounded even when a failed write includes an arbitrarily long path.
        const auto message = columns > 0 && string_width(toast.message) > columns * 2
                                 ? std::string("Details below: End / PgDn")
                                 : toast.message;
        rows.push_back(
            SettingsText(message, columns) |
            color(toast.semantic_role
                      ? loom::ui::design::tokens::token_by_role(pal, *toast.semantic_role)
                      : (toast.color == Color{} ? pal.success : toast.color)) |
            bold);
    }
    rows.push_back(SettingsFlow({text("←→ sections"), text("Tab/↑↓ fields"),
                                 text("PgUp/PgDn scroll"), text("Enter change")},
                                columns) |
                   color(pal.muted));
    return vbox(std::move(rows));
}

// ============================================================
// Interactive component
// ============================================================

/// Create the settings dialog with a live ConfigManager reference.
/// All reads and writes flow through `cfg`.
[[nodiscard]] inline Component MakeSettingsDialog(ConfigManager& cfg,
                                                  SettingsDialogOptions opts = {}) {

    struct State {
        SettingsDialogOptions opts;
        ConfigManager* cfg;
        WorkingSettings working;
        WorkingSettings defaults;  // pristine snapshot for Reset
        SettingsTabId selected_tab;
        int focus_row = 0;
        int manual_line = -1;
        int document_rows = 1;
        int focused_line = 0;
        Box viewport;
        std::array<Box, 3> action_boxes{{{1, 0, 1, 0}, {1, 0, 1, 0}, {1, 0, 1, 0}}};
        ToastState toast;
        bool dirty = false;
    };

    auto state = std::make_shared<State>();
    state->opts = std::move(opts);
    state->cfg = &cfg;
    state->working = snapshot_from(cfg);
    state->defaults = state->working;
    state->selected_tab = state->opts.initial_tab;

    // Helper: toggle boolean + mark dirty
    auto toggle = [state](bool& field) {
        field = !field;
        state->dirty = true;
    };
    auto mark = [state]() { state->dirty = true; };

    // Helper: set transient toast
    auto show_toast = [state](std::string msg, loom::ui::design::tokens::Role role =
                                                   loom::ui::design::tokens::Role::Success) {
        state->toast.message = std::move(msg);
        state->toast.semantic_role = role;
        state->toast.until = role == loom::ui::design::tokens::Role::Danger
                                 ? std::chrono::steady_clock::time_point::max()
                                 : std::chrono::steady_clock::now() + std::chrono::seconds(3);
    };

    auto save = [state, show_toast] {
        apply_to(state->working, *state->cfg);
        auto result = state->cfg->save();
        if (result) {
            state->dirty = false;
            state->defaults = state->working;
            show_toast("✓ Settings saved");
        } else {
            show_toast("✗ Save failed: " + result.error().message,
                       loom::ui::design::tokens::Role::Danger);
        }
    };
    auto reset = [state, show_toast] {
        state->working = state->defaults;
        state->dirty = false;
        state->manual_line = -1;
        show_toast("↺ Reverted to last saved values");
    };
    auto close = [state] {
        if (state->opts.on_close)
            state->opts.on_close(state->dirty ? std::optional("Settings closed without saving")
                                              : std::nullopt,
                                 CommandResultDisplay::system);
    };

    return Renderer([state] {
               return std::make_shared<SettingsLayoutNode>([state](int allocated_columns,
                                                                   int /*rows*/) -> Element {
                   const bool compact = allocated_columns < 72;
                   const int panel_columns = std::max(1, allocated_columns - (compact ? 2 : 22));
                   const int columns = panel_columns;
                   const auto& pal = *loom::ui::design::theme::current_theme().palette;
                   Element content;
                   switch (state->selected_tab) {
                   case SettingsTabId::General:
                       content = RenderGeneralTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::Model:
                       content = RenderModelTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::API:
                       content = RenderAPITab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::Permissions:
                       content = RenderPermissionsTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::Tools:
                       content = RenderToolsTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::MCP:
                       content = RenderMCPTab(state->working, state->focus_row,
                                              state->focus_row - 1, columns);
                       break;
                   case SettingsTabId::LSP:
                       content = RenderLSPTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::Bridge:
                       content = RenderBridgeTab(state->working, state->focus_row, columns);
                       break;
                   case SettingsTabId::Hooks:
                       content = RenderHooksTab(columns);
                       break;
                   case SettingsTabId::Privacy:
                       content = RenderPrivacyTab(columns);
                       break;
                   case SettingsTabId::About:
                       content = RenderAboutTab(columns);
                       break;
                   case SettingsTabId::Status:
                       content = RenderStatusTab(state->opts.status_rows, columns);
                       break;
                   case SettingsTabId::Usage: {
                       UsageSnapshot snap;
                       if (state->opts.refresh_usage) {
                           state->opts.refresh_usage(snap);
                       }
                       content = RenderUsageTab(snap, columns);
                       break;
                   }
                   case SettingsTabId::_COUNT:
                       content = text("(empty)");
                       break;
                   }

                   if (state->toast.active() && string_width(state->toast.message) > columns * 2) {
                       content = vbox({content, separator(),
                                       SettingsText(state->toast.message, columns) |
                                           color(state->toast.semantic_role
                                                     ? loom::ui::design::tokens::token_by_role(
                                                           pal, *state->toast.semantic_role)
                                                     : pal.success)});
                   }
                   content->ComputeRequirement();
                   state->document_rows = std::max(1, content->requirement().min_y);
                   state->focused_line = content->requirement().selected_box.y_min;
                   if (state->manual_line >= 0) {
                       // Layout negotiation may visit a wider provisional box first.
                       // Clamp only the rendered focus, never the user's scroll position.
                       content = content | focusPosition(0, std::clamp(state->manual_line, 0,
                                                                       state->document_rows - 1));
                   }
                   auto body = vbox({
                       content | yframe | reflect(state->viewport) | yflex,
                       RenderFooter(state->toast, state->dirty, panel_columns,
                                    &state->action_boxes),
                   });
                   Element panel = body;
                   if (compact) {
                       panel = vbox({text(std::format(" {}  {}/{}", tab_label(state->selected_tab),
                                                      static_cast<int>(state->selected_tab) + 1,
                                                      static_cast<int>(SettingsTabId::_COUNT))) |
                                         bold,
                                     separator(), body | flex});
                   } else {
                       panel = hbox({RenderSidebar(state->selected_tab), body | flex});
                   }
                   return window(text(" Settings ") | bold | color(pal.primary), panel) |
                          color(pal.primary) | bgcolor(pal.background) | clear_under;
               });
           }) |
           CatchEvent([state, toggle, mark, show_toast, save, reset, close](Event event) -> bool {
               if (is_ctrl_key(event, 's')) {
                   save();
                   return true;
               }
               if (is_ctrl_key(event, 'r')) {
                   reset();
                   return true;
               }
               if (event == Event::Escape) {
                   close();
                   return true;
               }
               if (event.is_mouse() && event.mouse().button == Mouse::Left &&
                   event.mouse().motion == Mouse::Pressed) {
                   const auto& mouse = event.mouse();
                   for (int index = 0; index < 3; ++index) {
                       if (!state->action_boxes[index].Contain(mouse.x, mouse.y)) continue;
                       if (index == 0 && state->dirty) save();
                       if (index == 1) reset();
                       if (index == 2) close();
                       return true;
                   }
               }
               const auto row_count = [&] {
                   switch (state->selected_tab) {
                   case SettingsTabId::General:
                       return 7;
                   case SettingsTabId::Model:
                       return 2;
                   case SettingsTabId::API:
                       return 6;
                   case SettingsTabId::Permissions:
                       return 7;
                   case SettingsTabId::Tools:
                       return 6;
                   case SettingsTabId::MCP:
                       return 1 + static_cast<int>(state->working.mcp_servers.size());
                   case SettingsTabId::LSP:
                       return 2;
                   case SettingsTabId::Bridge:
                       return 5;
                   default:
                       return 0;
                   }
               }();
               const int page = std::max(1, state->viewport.y_max - state->viewport.y_min);
               if (event == Event::PageUp || event == Event::PageDown || event == Event::Home ||
                   event == Event::End ||
                   (row_count == 0 && (event == Event::ArrowUp || event == Event::ArrowDown))) {
                   int line = state->manual_line < 0 ? state->focused_line : state->manual_line;
                   if (event == Event::Home)
                       line = 0;
                   else if (event == Event::End)
                       line = state->document_rows - 1;
                   else if (event == Event::PageUp)
                       line -= page;
                   else if (event == Event::PageDown)
                       line += page;
                   else if (event == Event::ArrowUp)
                       --line;
                   else
                       ++line;
                   state->manual_line = std::clamp(line, 0, state->document_rows - 1);
                   return true;
               }
               // Field actions restore focus following; explicit document scrolling
               // remains stable until a selection or section actually changes.
               if (event == Event::Tab || event == Event::TabReverse || event == Event::ArrowLeft ||
                   event == Event::ArrowRight || event == Event::ArrowUp ||
                   event == Event::ArrowDown || event.is_character() || event == Event::Return)
                   state->manual_line = -1;

               // --- Tab cycling (1..9/0 hotkeys + Ctrl+Tab) ---
               if (event.is_character()) {
                   char ch = event.character()[0];
                   if (ch >= '1' && ch <= '9') {
                       int idx = ch - '1';
                       constexpr std::array<SettingsTabId, 9> kOrder = {{
                           SettingsTabId::General,
                           SettingsTabId::Model,
                           SettingsTabId::API,
                           SettingsTabId::Permissions,
                           SettingsTabId::Tools,
                           SettingsTabId::MCP,
                           SettingsTabId::LSP,
                           SettingsTabId::Bridge,
                           SettingsTabId::Hooks,
                       }};
                       if (idx < (int)kOrder.size()) {
                           state->selected_tab = kOrder[idx];
                           state->focus_row = 0;
                           return true;
                       }
                   }
                   if (ch == '0') {
                       state->selected_tab = SettingsTabId::Privacy;
                       state->focus_row = 0;
                       return true;
                   }
                   if ((ch == 'a' || ch == 'A') && state->selected_tab != SettingsTabId::MCP) {
                       state->selected_tab = SettingsTabId::About;
                       state->focus_row = 0;
                       return true;
                   }
               }

               // Usage tab refresh
               if (state->selected_tab == SettingsTabId::Usage && event == Event::Character('r')) {
                   show_toast("↻ Usage data refreshed");
                   return true;
               }

               // Cycle the actual fields of the current section.
               if (event == Event::Tab) {
                   state->focus_row = (state->focus_row + 1) % std::max(1, row_count);
                   return true;
               }
               if (event == Event::TabReverse) {
                   state->focus_row =
                       (state->focus_row + std::max(1, row_count) - 1) % std::max(1, row_count);
                   return true;
               }

               // Move between fields, continuing to adjacent sections at the ends.
               // Read-only sections use document scrolling above instead.
               if (event == Event::ArrowUp || event == Event::Character('k')) {
                   if (state->focus_row <= 0) {
                       // cycle tabs up
                       auto cur = static_cast<int>(state->selected_tab);
                       cur = (cur + static_cast<int>(SettingsTabId::_COUNT) - 1) %
                             static_cast<int>(SettingsTabId::_COUNT);
                       state->selected_tab = static_cast<SettingsTabId>(cur);
                   } else {
                       state->focus_row--;
                   }
                   return true;
               }
               if (event == Event::ArrowDown || event == Event::Character('j')) {
                   if (state->focus_row >= row_count - 1) {
                       // cycle tabs down
                       auto cur = static_cast<int>(state->selected_tab);
                       cur = (cur + 1) % static_cast<int>(SettingsTabId::_COUNT);
                       state->selected_tab = static_cast<SettingsTabId>(cur);
                       state->focus_row = 0;
                   } else {
                       state->focus_row++;
                   }
                   return true;
               }

               // Left / Right also cycle tabs
               if (event == Event::ArrowLeft) {
                   auto cur = static_cast<int>(state->selected_tab);
                   cur = (cur + static_cast<int>(SettingsTabId::_COUNT) - 1) %
                         static_cast<int>(SettingsTabId::_COUNT);
                   state->selected_tab = static_cast<SettingsTabId>(cur);
                   state->focus_row = 0;
                   return true;
               }
               if (event == Event::ArrowRight) {
                   auto cur = static_cast<int>(state->selected_tab);
                   cur = (cur + 1) % static_cast<int>(SettingsTabId::_COUNT);
                   state->selected_tab = static_cast<SettingsTabId>(cur);
                   state->focus_row = 0;
                   return true;
               }

               // --- Space / Enter: toggle boolean row ---
               if (event == Event::Character(' ') || event == Event::Return) {
                   auto& w = state->working;
                   auto row = state->focus_row;
                   switch (state->selected_tab) {
                   // General booleans / cyclic selects
                   case SettingsTabId::General:
                       if (row == 0) {  // Theme: cycle
                           {
                               static const std::vector<std::string> kThemes = {
                                   "dark", "light", "high-contrast", "auto"};
                               auto it = std::find(kThemes.begin(), kThemes.end(), w.theme);
                               int idx = (it == kThemes.end())
                                             ? 0
                                             : static_cast<int>(it - kThemes.begin());
                               w.theme = kThemes[(idx + 1) % kThemes.size()];
                               mark();
                               return true;
                           }
                       }
                       if (row == 1) {
                           toggle(w.auto_mode);
                           return true;
                       }
                       if (row == 2) {  // effort cycle
                           static const std::vector<std::string> kEffort = {"low", "medium",
                                                                            "high"};
                           auto it = std::find(kEffort.begin(), kEffort.end(), w.effort_level);
                           int idx =
                               (it == kEffort.end()) ? 1 : static_cast<int>(it - kEffort.begin());
                           w.effort_level = kEffort[(idx + 1) % kEffort.size()];
                           mark();
                           return true;
                       }
                       if (row == 3) {
                           toggle(w.fast_mode);
                           return true;
                       }
                       if (row == 4) {
                           toggle(w.show_thinking);
                           return true;
                       }
                       if (row == 5) {
                           toggle(w.show_token_usage);
                           return true;
                       }
                       if (row == 6) {
                           toggle(w.compact_mode);
                           return true;
                       }
                       break;
                   case SettingsTabId::Model:
                       if (row == 0) {  // cycle model — no built-in presets, no-op
                           mark();
                           return true;
                       }
                       if (row == 1) {  // teammate model cycle — no built-in presets, no-op
                           mark();
                           return true;
                       }
                       break;
                   case SettingsTabId::API:
                       if (row == 2) {  // timeout step
                           w.timeout_seconds += 30;
                           if (w.timeout_seconds > 600) w.timeout_seconds = 30;
                           mark();
                           return true;
                       }
                       if (row == 3) {
                           toggle(w.verify_ssl);
                           return true;
                       }
                       if (row == 0) {  // cycle base URL presets
                           static const std::vector<std::optional<std::string>> kUrls = {
                               std::nullopt,
                               std::optional("http://localhost:8080"),
                           };
                           auto it = std::find(kUrls.begin(), kUrls.end(), w.base_url);
                           int idx = (it == kUrls.end()) ? 0 : static_cast<int>(it - kUrls.begin());
                           w.base_url = kUrls[(idx + 1) % kUrls.size()];
                           mark();
                           return true;
                       }
                       if (row == 1) {
                           if (w.api_key) {
                               w.api_key = std::nullopt;
                               mark();
                               show_toast("API key cleared");
                           } else {
                               show_toast("Set LOOM_API_KEY or run the auth flow",
                                          loom::ui::design::tokens::Role::Warning);
                           }
                           return true;
                       }
                       if (row == 4) {  // proxy: cycle presets
                           static const std::vector<std::optional<std::string>> kProxy = {
                               std::nullopt,
                               std::optional("http://localhost:8888"),
                               std::optional("http://proxy.corp:3128"),
                           };
                           auto it = std::find(kProxy.begin(), kProxy.end(), w.proxy);
                           int idx =
                               (it == kProxy.end()) ? 0 : static_cast<int>(it - kProxy.begin());
                           w.proxy = kProxy[(idx + 1) % kProxy.size()];
                           mark();
                           return true;
                       }
                       if (row == 5) {  // max retries step
                           w.max_retries = (w.max_retries % 10) + 1;
                           mark();
                           return true;
                       }
                       break;
                   case SettingsTabId::Permissions:
                       if (row == 0) {  // cycle default perm mode
                           auto m = static_cast<int>(w.default_perm);
                           w.default_perm = static_cast<DefaultPermissionMode>((m + 1) % 3);
                           mark();
                           return true;
                       }
                       if (row == 1) {
                           toggle(w.allow_bash);
                           return true;
                       }
                       if (row == 2) {
                           toggle(w.allow_file_write);
                           return true;
                       }
                       if (row == 3) {
                           toggle(w.allow_network);
                           return true;
                       }
                       break;
                   case SettingsTabId::Tools:
                       if (row == 0) {
                           toggle(w.enable_agent_tool);
                           return true;
                       }
                       if (row == 1) {
                           toggle(w.enable_web_fetch);
                           return true;
                       }
                       if (row == 2) {
                           toggle(w.enable_web_search);
                           return true;
                       }
                       if (row == 3) {
                           toggle(w.enable_bash_granular);
                           return true;
                       }
                       if (row == 4) {
                           toggle(w.enable_glob_grep_safety);
                           return true;
                       }
                       if (row == 5) {
                           toggle(w.enable_skill_loading);
                           return true;
                       }
                       break;
                   case SettingsTabId::MCP:
                       if (row == 0) {
                           show_toast("Use /mcp for server lifecycle and status");
                           return true;
                       }
                       break;
                   case SettingsTabId::LSP:
                       if (row == 0) {
                           toggle(w.enable_lsp);
                           return true;
                       }
                       if (row == 1) {
                           w.lsp_timeout += 15;
                           if (w.lsp_timeout > 300) w.lsp_timeout = 15;
                           mark();
                           return true;
                       }
                       break;
                   case SettingsTabId::Bridge:
                       if (row == 0) {
                           toggle(w.enable_bridge);
                           return true;
                       }
                       if (row == 1) {
                           toggle(w.bridge_outbound_only);
                           return true;
                       }
                       if (row == 2) {
                           w.bridge_port += 1;
                           mark();
                           return true;
                       }
                       break;
                   default:
                       break;
                   }
               }

               // MCP-specific keys
               if (state->selected_tab == SettingsTabId::MCP) {
                   if (event == Event::Character('a')) {
                       show_toast("Use /mcp add to configure a server",
                                  loom::ui::design::tokens::Role::Warning);
                       return true;
                   }
                   if (event == Event::Character('r')) {
                       if (state->focus_row > 0 && !state->working.mcp_servers.empty()) {
                           int idx = std::clamp(state->focus_row - 1, 0,
                                                (int)state->working.mcp_servers.size() - 1);
                           state->working.mcp_servers.erase(state->working.mcp_servers.begin() +
                                                            idx);
                           state->focus_row =
                               std::min(state->focus_row,
                                        static_cast<int>(state->working.mcp_servers.size()));
                           mark();
                           show_toast("− MCP server removed");
                           return true;
                       }
                   }
               }

               return false;
           });
}

}  // namespace loom::ui::dialogs::settings_dialog
