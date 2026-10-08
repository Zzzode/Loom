// app_settings.cpp — plain impl unit owning the SettingsState nested PIMPL.
// Keeps loom.config.settings_manager out of both app.cppm and the :impl BMI.
module;


module loom.ui.app.app;

import std;

import loom.config.settings_manager;
import loom.config.config;             // ConfigManager for statusLine.segments
import loom.ui.foundation.theme_provider;

namespace loom::ui {

namespace settings_nm = loom::utils::settings_manager;

struct SettingsState {
    std::unique_ptr<settings_nm::SettingsManager> manager;
    settings_nm::UnsubscribeFn unsubscribe;
};

void SettingsStateDeleter::operator()(SettingsState* p) const noexcept {
    delete p;
}

void AppAdapter::construct_settings() {
    settings_.reset(new SettingsState());
}

void AppAdapter::init_settings_manager() {
    if (!settings_) return;
    settings_->manager = std::make_unique<settings_nm::SettingsManager>();
    settings_->manager->initialize();
}

void AppAdapter::subscribe_settings_changed(std::function<void()> cb) {
    if (!settings_ || !settings_->manager) return;
    settings_->unsubscribe =
        settings_->manager->on_change([cb = std::move(cb)](
            settings_nm::SettingSource) mutable { cb(); });
}

std::optional<std::string> AppAdapter::setting_string(std::string_view key) const {
    if (!settings_ || !settings_->manager) return std::nullopt;
    auto settings = settings_->manager->get_initial_settings();
    auto it = settings.find(std::string(key));
    if (it != settings.end() &&
        std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return std::nullopt;
}

std::optional<std::string> AppAdapter::statusline_setting(std::string_view key) const {
    if (!settings_ || !settings_->manager) return std::nullopt;
    auto settings = settings_->manager->get_initial_settings();
    auto sl = settings.find("statusLine");
    if (sl == settings.end() ||
        !std::holds_alternative<std::map<std::string, std::string>>(sl->second)) {
        return std::nullopt;
    }
    const auto& m = std::get<std::map<std::string, std::string>>(sl->second);
    auto it = m.find(std::string(key));
    return it != m.end() ? std::optional<std::string>{it->second} : std::nullopt;
}

std::string AppAdapter::output_style_setting() const {
    return setting_string("outputStyle").value_or("full");
}

void AppAdapter::ProjectSettingsToScreenState() {
    if (!settings_ || !settings_->manager) return;

    // --- default model ---
    screen_state_->settings_model = setting_string("model").value_or(std::string{});

    // --- default agent display name ---
    screen_state_->settings_agent_name =
        setting_string("agent").value_or(std::string{});

    // --- status line config (settings.statusLine) ---
    std::optional<std::string> status_line_type = statusline_setting("type");
    std::string status_line_command  = statusline_setting("command").value_or(std::string{});
    std::optional<bool> status_line_enabled;
    int status_line_padding = 0;

    if (auto enabled = statusline_setting("enabled")) {
        status_line_enabled = parse_bool_text(*enabled);
    }
    if (auto pad = statusline_setting("padding")) {
        if (auto parsed = parse_int_text(*pad)) status_line_padding = *parsed;
    }

    if (auto command = first_non_empty_env({
            "LOOM_STATUS_LINE_COMMAND",
            "LOOM_STATUS_LINE_COMMAND"})) {
        status_line_command = *command;
        status_line_type = "command";
    }
    if (auto enabled = first_non_empty_env({
            "LOOM_STATUS_LINE_ENABLED",
            "LOOM_STATUS_LINE_ENABLED"})) {
        status_line_enabled = parse_bool_text(*enabled);
    }
    if (auto padding = first_non_empty_env({
            "LOOM_STATUS_LINE_PADDING",
            "LOOM_STATUS_LINE_PADDING"})) {
        if (auto parsed = parse_int_text(*padding)) {
            status_line_padding = *parsed;
        }
    }

    const bool type_allows_command = !status_line_type || *status_line_type == "command";
    const bool enabled = status_line_enabled.value_or(
        !status_line_command.empty() && type_allows_command);
    screen_state_->status_line_command = std::move(status_line_command);
    screen_state_->status_line_padding = status_line_padding;
    screen_state_->status_line_enabled =
        enabled && type_allows_command && !screen_state_->status_line_command.empty();
    if (!screen_state_->status_line_enabled) {
        screen_state_->status_line_text.clear();
    }

    // ── Built-in status bar (statusLine.enabled + statusLine.segments) ──
    // The SettingsManager flattens nested objects and drops non-string
    // values, so the bool-valued `segments` sub-object cannot survive
    // that path.  Read from ConfigManager instead, which has full type
    // support.  Thread-local + reload-on-call mirrors the settings-dialog
    // fallback pattern (repl_screen_dialog_panels.cpp).
    static thread_local loom::core::ConfigManager bar_cfg;
    (void)bar_cfg.load(loom::core::LoadOptions{.quiet = true});

    // ── Color theme ──────────────────────────────────────────────────────
    // LOOM_THEME env var overrides the config file; both default to "auto"
    // (currently equivalent to dark, pending OSC-11 detection).
    {
        std::string theme_name = bar_cfg.settings().display.theme;
        if (auto env = first_non_empty_env({"LOOM_THEME", "LOOM_THEME"})) {
            theme_name = *env;
        }
        loom::ui::design::theme::set_theme(
            loom::ui::design::theme::parse_variant(theme_name));
    }

    const auto& sl = bar_cfg.settings().status_line;
    screen_state_->status_bar_enabled = sl.enabled.value_or(true);
    auto& segs = screen_state_->status_bar_segments;
    if (auto it = sl.segments.find("cwd");    it != sl.segments.end()) segs.cwd    = it->second;
    if (auto it = sl.segments.find("git");    it != sl.segments.end()) segs.git    = it->second;
    if (auto it = sl.segments.find("model");  it != sl.segments.end()) segs.model  = it->second;
    if (auto it = sl.segments.find("tokens"); it != sl.segments.end()) segs.tokens = it->second;
    if (auto it = sl.segments.find("cost");   it != sl.segments.end()) segs.cost   = it->second;
    if (auto it = sl.segments.find("tasks");  it != sl.segments.end()) segs.tasks  = it->second;
    if (auto it = sl.segments.find("agent");  it != sl.segments.end()) segs.agent  = it->second;
}


}  // namespace loom::ui
