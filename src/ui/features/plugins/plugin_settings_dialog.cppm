/// @file plugin_settings_dialog.cppm
/// @brief Per-plugin settings: global auto-update toggle, per-plugin
///        enable/disable, per-plugin options, and keybindings editor
///        (name + hotkey input with add/remove).
///
/// Data comes from C1: plugin_ui_data::InstalledCellData and
/// plugin_ui_data::ConfigStep — no direct engine reads here.
module;

#include <cstdint>
#include <cstddef>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

export module loom.ui.features.plugins.plugin_settings_dialog;

import loom.ui.foundation.theme_provider;

import std;

import loom.ui.foundation.design_figures;

import loom.types.types;
import loom.commands.plugin_ui_data;

export namespace loom::ui::plugins::plugin_settings_dialog {
using namespace ftxui;

namespace ui = loom::commands::plugin_ui;

using ui::InstalledCellData;
using ui::ConfigStep;

// =========================================================================
// Keybinding entry (name → hotkey string)
// =========================================================================

struct KeybindingEntry {
    std::string name;        // e.g. "toggle-format", "send-chunk"
    std::string hotkey;      // e.g. "Ctrl+Alt+F", "⌘K"
    std::string scope;       // "global" / "plugin-local"
    bool        enabled = true;
};

// =========================================================================
// Per-plugin options row (schema name → current value)
// =========================================================================

struct PluginOption {
    std::string name;
    std::string current_value;
    std::string type_hint;   // "string" / "bool" / "number" / "select"
    bool        required = false;
    bool        is_secret = false;
    std::vector<std::string> allowed_values;  // for "select" type
};

struct PerPluginSettings {
    std::string plugin_id;
    bool        enabled = true;
    bool        auto_update = true;
    std::vector<PluginOption> options;
    std::vector<KeybindingEntry> keybindings;
    std::vector<ConfigStep> pending_steps;   // from C1::build_config_steps
};

// =========================================================================
// Global settings
// =========================================================================

struct GlobalPluginSettings {
    bool auto_update_all = true;
    bool allow_unverified = false;
    bool allow_unsafe_permissions = false;
    bool show_trust_warning_on_install = true;
    std::string default_install_scope = "user";   // user / project / local
};

// =========================================================================
// Dialog inputs (all from C1 prep)
// =========================================================================

struct SettingsDialogInputs {
    GlobalPluginSettings          globals;
    std::vector<PerPluginSettings> per_plugin;   // one per installed plugin

    // Routed focus hint
    std::optional<std::string> focus_plugin_id;

    // Callbacks
    std::function<void(const GlobalPluginSettings&)> on_save_globals;
    std::function<void(std::string_view plugin_id,
                       const PerPluginSettings&)>     on_save_plugin;
    std::function<void(std::string_view plugin_id,
                       const KeybindingEntry& kb)>    on_update_keybinding;
    std::function<void(std::string_view plugin_id,
                       std::string_view kb_name)>      on_remove_keybinding;
    std::function<void()>                              on_close;
};

// =========================================================================
// Sub-tabs inside the settings dialog
// =========================================================================

enum class SettingsTab : unsigned char {
    Global,      // global auto-update + trust settings
    Plugins,     // per-plugin list (enable / auto-update toggles)
    Options,     // per-plugin options (schema values)
    Keybindings, // hotkey editor
};

inline constexpr const char* k_settings_tab_names[] = {
    "Global", "Plugins", "Options", "Keybindings",
};

// =========================================================================
// Rendering helpers
// =========================================================================

namespace detail {

// Render a boolean toggle row.
[[nodiscard]] inline Element BoolToggleRow(
    std::string_view label, std::string_view hint,
    bool value, bool selected, Color accent = loom::ui::design::theme::current_theme().palette->info)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto val = text(value ? " [✓ ON] " : " [  OFF] ")
        | color(value ? pal.success : pal.muted) | bold;
    auto row = hbox({
        text(selected ? std::string(loom::ui::design::figures::kPointerSmall) + " " : "  ") | color(accent),
        text(std::string{label}) | bold
             | color(selected ? pal.text : pal.muted),
        filler(),
        std::move(val),
    });
    if (selected) row = row | bgcolor(pal.surface_tint_3);
    Elements out{std::move(row)};
    if (!hint.empty()) {
        out.push_back(paragraph("   " + std::string{hint}) | color(pal.muted));
    }
    return vbox(std::move(out));
}

// Render the 4-tab header
[[nodiscard]] inline Element RenderSettingsTabBar(SettingsTab active) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements bits;
    for (unsigned i = 0; i < 4; ++i) {
        const bool sel = (i == (unsigned)active);
        bits.push_back(text(" " + std::string{k_settings_tab_names[i]} + " ")
            | (sel ? (bold | color(pal.info) | bgcolor(pal.message_actions_background))
                   : color(pal.muted)));
        if (i + 1 < 4) bits.push_back(text("│") | dim);
    }
    return hbox(std::move(bits));
}

// ---- Global tab ----

[[nodiscard]] inline Element RenderGlobalTab(
    const GlobalPluginSettings& g, int selected_row)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    // Rows: auto-update-all, default-scope, allow-unverified,
    //       allow-unsafe, show-trust-warning
    return vbox({
        hbox({
            text(" Global Plugin Settings ") | bold | color(pal.info),
            filler(),
            text("Applies to all plugins") | color(pal.muted),
        }),
        separator(),
        text(""),
        BoolToggleRow("Auto-update all plugins",
            "When enabled, periodically checks for updates and installs them.",
            g.auto_update_all, selected_row == 0, pal.success),
        separator() | dim,
        // Default install scope (pseudo-toggle: cycle through user/project/local)
        hbox({
            text(selected_row == 1 ? std::string(loom::ui::design::figures::kPointerSmall) + " " : "  ") | color(pal.warning),
            text("Default install scope") | bold
                 | color(selected_row == 1 ? pal.text : pal.muted),
            filler(),
            text(" [" + g.default_install_scope + "] ") | bold | color(pal.warning),
        }) | (selected_row == 1 ? bgcolor(pal.warning_tint_bg) : nothing),
        paragraph("   New plugin installations use this scope unless overridden.") | color(pal.muted),
        separator() | dim,
        BoolToggleRow("Allow unverified plugins",
            "Install plugins that are not signed or verified by a trusted marketplace.",
            g.allow_unverified, selected_row == 2, pal.warning),
        separator() | dim,
        BoolToggleRow("Allow elevated permissions",
            "Permit plugins to request 'unsafe' permissions (bash, exec, file write).",
            g.allow_unsafe_permissions, selected_row == 3, pal.danger),
        separator() | dim,
        BoolToggleRow("Show trust warning on install",
            "Display the security trust dialog before every plugin install/update.",
            g.show_trust_warning_on_install, selected_row == 4, pal.info),
        text(""),
        separator(),
        hbox({
            text(" ↑↓/j/k") | bold | color(pal.info), text(" navigate  "),
            text("Space") | bold | color(pal.info), text(" toggle  "),
            text("⏎") | bold | color(pal.info), text(" save  "),
            text("Esc") | bold | color(pal.info), text(" close"),
            filler(),
            text("[Ctrl+S] save all") | color(pal.muted),
        }) | color(pal.muted),
    });
}

// ---- Plugins tab (per-plugin enable/auto-update) ----

[[nodiscard]] inline Element RenderPerPluginRow(
    const PerPluginSettings& p, bool selected, bool is_focus_target)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto enabled_dot = text(p.enabled ? std::string(loom::ui::design::figures::kBullet) + " " : std::string(loom::ui::design::figures::kEffortLow) + " ")
        | color(p.enabled ? pal.success : pal.muted);
    auto au_dot = text(p.auto_update ? std::string(loom::ui::design::figures::kArrowUp) + " " : "  ")
        | color(p.auto_update ? pal.info : pal.muted);

    auto row = hbox({
        text(is_focus_target ? "★ " : (selected ? std::string(loom::ui::design::figures::kPointerSmall) + " " : "  "))
            | color(is_focus_target ? pal.warning : pal.info),
        enabled_dot,
        text(p.plugin_id) | bold
             | color(selected ? pal.text : pal.muted),
        filler(),
        au_dot,
        text(p.auto_update ? "auto-update" : "manual-only") | color(pal.muted),
        text("  "),
        text("[" + std::to_string(p.options.size()) + " opt]") | color(pal.muted),
        text(" "),
        text("[" + std::to_string(p.keybindings.size()) + " key]") | color(pal.muted),
    });
    if (selected) row = row | bgcolor(pal.info_tint_bg);
    return row;
}

[[nodiscard]] inline Element RenderPluginsTab(
    const std::vector<PerPluginSettings>& all,
    int selected,
    std::optional<std::string> focus_id)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements rows;
    rows.push_back(hbox({
        text(" Per-Plugin Toggles ") | bold | color(pal.primary),
        filler(),
        text(std::format("{} plugin{} installed",
                         all.size(), all.size() == 1 ? "" : "s")) | color(pal.muted),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" ") | dim,
        text("Status") | color(pal.muted) | size(WIDTH, EQUAL, 8),
        text("Plugin") | color(pal.muted),
        filler(),
        text("Updates") | color(pal.muted) | size(WIDTH, EQUAL, 14),
        text("Opts") | color(pal.muted) | size(WIDTH, EQUAL, 6),
        text("Keys") | color(pal.muted) | size(WIDTH, EQUAL, 6),
    }));
    rows.push_back(separator() | dim);

    if (all.empty()) {
        rows.push_back(text("  No plugins installed yet.") | color(pal.muted) | center);
    } else {
        for (int i = 0; i < (int)all.size(); ++i) {
            const bool is_target =
                focus_id.has_value() && all[i].plugin_id == *focus_id;
            rows.push_back(RenderPerPluginRow(all[i], i == selected, is_target));
            if (i + 1 < (int)all.size()) rows.push_back(separator() | dim);
        }
    }

    rows.push_back(separator());
    rows.push_back(hbox({
        text(" ↑↓/j/k") | bold | color(pal.info), text(" navigate  "),
        text("Space") | bold | color(pal.info), text(" toggle enable  "),
        text("U") | bold | color(pal.info), text(" toggle auto-update  "),
        text("→") | bold | color(pal.info), text(" go to options/keys"),
    }) | color(pal.muted));
    return vbox(std::move(rows));
}

// ---- Options tab (schema values for the focused plugin) ----

[[nodiscard]] inline Element RenderOptionRow(
    const PluginOption& opt, bool selected)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    std::string display_value;
    if (opt.is_secret && !opt.current_value.empty()) {
        display_value.assign(opt.current_value.size(), '*');
    } else {
        display_value = opt.current_value.empty() ? "(unset)" : opt.current_value;
    }
    auto row = hbox({
        text(selected ? std::string(loom::ui::design::figures::kPointerSmall) + " " : "  ") | color(pal.success),
        text(opt.name) | bold
             | color(selected ? pal.text : pal.muted),
        opt.required ? text(" *") | color(pal.danger) : text(""),
        text(" [" + opt.type_hint + "]") | color(pal.muted),
        filler(),
        text(display_value) | color(selected ? pal.text : pal.info),
    });
    if (selected) row = row | bgcolor(pal.success_tint_bg);
    return row;
}

[[nodiscard]] inline Element RenderOptionsTab(
    const PerPluginSettings* focused, int selected_opt)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements rows;
    rows.push_back(hbox({
        text(" Plugin Options ") | bold | color(pal.info),
        filler(),
        focused ? text(focused->plugin_id) | color(pal.info) | underlined
                : text("(select a plugin first)") | color(pal.muted),
    }));
    rows.push_back(separator());

    if (!focused || focused->options.empty()) {
        rows.push_back(text("  No options to configure.") | color(pal.muted) | center);
    } else {
        // Pending config steps banner
        if (!focused->pending_steps.empty()) {
            rows.push_back(vbox({
                hbox({
                    text(" " + std::string(loom::ui::design::figures::kInfo) + " ") | color(pal.warning),
                    text(std::format("{} configuration step{} required before first use",
                                     focused->pending_steps.size(),
                                     focused->pending_steps.size() == 1 ? "" : "s"))
                        | bold | color(pal.warning),
                }),
                paragraph("   These options are required.  Fill them in to enable the plugin.") | color(pal.muted),
            }) | borderLight | color(pal.warning));
            rows.push_back(text(""));
        }

        for (int i = 0; i < (int)focused->options.size(); ++i) {
            rows.push_back(RenderOptionRow(focused->options[i], i == selected_opt));
            if (i + 1 < (int)focused->options.size())
                rows.push_back(separator() | dim);
        }
    }

    rows.push_back(separator());
    rows.push_back(hbox({
        text(" ↑↓/j/k") | bold | color(pal.info), text(" navigate  "),
        text("⏎") | bold | color(pal.info), text(" edit value  "),
        text("←") | bold | color(pal.info), text(" back to plugins list"),
    }) | color(pal.muted));
    return vbox(std::move(rows));
}

// ---- Keybindings tab ----

[[nodiscard]] inline Element RenderKeybindingRow(
    const KeybindingEntry& kb, bool selected)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto row = hbox({
        text(selected ? std::string(loom::ui::design::figures::kPointerSmall) + " " : "  ") | color(pal.info),
        text(kb.enabled ? std::string(loom::ui::design::figures::kBullet) + " " : std::string(loom::ui::design::figures::kEffortLow) + " ")
            | color(kb.enabled ? pal.success : pal.muted),
        text(kb.name) | bold
             | color(selected ? pal.text : pal.muted),
        filler(),
        text("[" + kb.hotkey + "]") | bold | color(pal.info),
        text(" "),
        text(kb.scope) | color(pal.muted),
    });
    if (selected) row = row | bgcolor(pal.info_tint_bg);
    return row;
}

[[nodiscard]] inline Element RenderKeybindingsTab(
    const PerPluginSettings* focused, int selected_kb,
    bool editing_new, std::string_view new_name, std::string_view new_hotkey)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements rows;
    rows.push_back(hbox({
        text(" Keybindings Editor ") | bold | color(pal.info),
        filler(),
        focused ? text(focused->plugin_id) | color(pal.info) | underlined
                : text("(select a plugin first)") | color(pal.muted),
    }));
    rows.push_back(separator());

    if (!focused || focused->keybindings.empty()) {
        rows.push_back(text("  No keybindings yet.  Press [A] to add one.")
                        | color(pal.muted) | center);
    } else {
        for (int i = 0; i < (int)focused->keybindings.size(); ++i) {
            rows.push_back(RenderKeybindingRow(focused->keybindings[i],
                                               i == selected_kb));
            if (i + 1 < (int)focused->keybindings.size())
                rows.push_back(separator() | dim);
        }
    }

    // Add-new inline editor
    if (editing_new) {
        rows.push_back(separator());
        rows.push_back(text(" New keybinding:") | bold | color(pal.info));
        rows.push_back(hbox({
            text("  name: ") | color(pal.muted),
            text(std::string{new_name.empty() ? "(type name)" : std::string{new_name}})
                 | color(new_name.empty() ? pal.muted : pal.text),
            text("│") | (loom::ui::design::theme::current_theme().a11y.reduced_motion ? nothing : blink) | color(pal.info),
            filler(),
            text("hotkey: ") | color(pal.muted),
            text(std::string{new_hotkey.empty() ? "(press key combo)"
                                               : std::string{new_hotkey}})
                 | color(new_hotkey.empty() ? pal.muted : pal.info) | bold,
        }) | borderLight | color(pal.info));
    }

    rows.push_back(separator());
    rows.push_back(hbox({
        text(" ↑↓/j/k") | bold | color(pal.info), text(" navigate  "),
        text("A") | bold | color(pal.info), text("dd  "),
        text("R") | bold | color(pal.info), text("emove  "),
        text("Space") | bold | color(pal.info), text(" enable/disable  "),
        text("⏎") | bold | color(pal.info), text(" edit hotkey"),
    }) | color(pal.muted));
    return vbox(std::move(rows));
}

} // namespace detail

// =========================================================================
// Interactive component
// =========================================================================

struct SettingsState {
    SettingsDialogInputs inputs;
    SettingsTab          active_tab = SettingsTab::Global;

    // Per-tab selection indices
    int global_selected = 0;         // 0..4 rows
    int plugin_selected = 0;
    int option_selected = 0;
    int keybinding_selected = 0;

    // Keybindings editor state
    bool        kb_adding = false;
    int         kb_edit_field = 0;    // 0 = name, 1 = hotkey
    std::string kb_new_name;
    std::string kb_new_hotkey;

    // Currently-focused plugin pointer (for Options/Keybindings tabs)
    std::optional<std::size_t> focused_plugin_index;
};

[[nodiscard]] inline Component MakePluginSettingsDialog(
    SettingsDialogInputs inputs)
{
    auto state = std::make_shared<SettingsState>();
    state->inputs = std::move(inputs);

    // Auto-focus a plugin if routed
    if (state->inputs.focus_plugin_id) {
        for (std::size_t i = 0; i < state->inputs.per_plugin.size(); ++i) {
            if (state->inputs.per_plugin[i].plugin_id == *state->inputs.focus_plugin_id) {
                state->focused_plugin_index = i;
                state->plugin_selected = (int)i;
                state->active_tab = SettingsTab::Plugins;
                break;
            }
        }
    }

    // Build an embedded input component for option/keybind editing
    auto option_input = std::make_shared<std::string>();
    Component option_input_widget = Input(option_input.get(), "Enter value…");

    return Renderer([state] {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        Element body;
        switch (state->active_tab) {
            case SettingsTab::Global:
                body = detail::RenderGlobalTab(state->inputs.globals,
                                               state->global_selected);
                break;
            case SettingsTab::Plugins:
                body = detail::RenderPluginsTab(
                    state->inputs.per_plugin, state->plugin_selected,
                    state->inputs.focus_plugin_id);
                break;
            case SettingsTab::Options: {
                const PerPluginSettings* p = nullptr;
                if (state->focused_plugin_index &&
                    *state->focused_plugin_index < state->inputs.per_plugin.size())
                    p = &state->inputs.per_plugin[*state->focused_plugin_index];
                body = detail::RenderOptionsTab(p, state->option_selected);
                break;
            }
            case SettingsTab::Keybindings: {
                const PerPluginSettings* p = nullptr;
                if (state->focused_plugin_index &&
                    *state->focused_plugin_index < state->inputs.per_plugin.size())
                    p = &state->inputs.per_plugin[*state->focused_plugin_index];
                body = detail::RenderKeybindingsTab(
                    p, state->keybinding_selected,
                    state->kb_adding, state->kb_new_name, state->kb_new_hotkey);
                break;
            }
        }

        return vbox({
            hbox({
                text(" Plugin Settings ") | bold | color(pal.info),
                filler(),
                detail::RenderSettingsTabBar(state->active_tab),
            }),
            separator() | color(pal.info),
            std::move(body) | flex,
        }) | borderDouble | bgcolor(pal.surface_tint_3);

    }) | CatchEvent([state, option_input](Event event) -> bool {
        // Tab switching: 1/2/3/4 digits OR Ctrl+[ Left / Right arrows
        if (event == Event::Tab) {
            state->active_tab = (SettingsTab)(((unsigned)state->active_tab + 1) % 4);
            return true;
        }
        if (event.is_character()) {
            const char ch = event.character()[0];
            if (ch >= '1' && ch <= '4') {
                state->active_tab = (SettingsTab)(unsigned)(ch - '1');
                return true;
            }
        }

        // Global close
        if (event == Event::Escape) {
            if (state->kb_adding) { state->kb_adding = false; return true; }
            if (state->inputs.on_close) state->inputs.on_close();
            return true;
        }

        // Ctrl+S → save all globals + per-plugin
        if (event.input() == "ctrl+s") {
            if (state->inputs.on_save_globals)
                state->inputs.on_save_globals(state->inputs.globals);
            for (const auto& p : state->inputs.per_plugin) {
                if (state->inputs.on_save_plugin)
                    state->inputs.on_save_plugin(p.plugin_id, p);
            }
            return true;
        }

        // --- Per-tab handlers ------------------------------------------------
        switch (state->active_tab) {
            // --- Global tab ---
            case SettingsTab::Global: {
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    state->global_selected = std::max(0, state->global_selected - 1);
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    state->global_selected = std::min(4, state->global_selected + 1);
                    return true;
                }
                if (event == Event::Character(' ')) {
                    auto& g = state->inputs.globals;
                    switch (state->global_selected) {
                        case 0: g.auto_update_all = !g.auto_update_all; break;
                        case 1: {
                            // Cycle default scope
                            if (g.default_install_scope == "user")    g.default_install_scope = "project";
                            else if (g.default_install_scope == "project") g.default_install_scope = "local";
                            else g.default_install_scope = "user";
                            break;
                        }
                        case 2: g.allow_unverified = !g.allow_unverified; break;
                        case 3: g.allow_unsafe_permissions = !g.allow_unsafe_permissions; break;
                        case 4: g.show_trust_warning_on_install = !g.show_trust_warning_on_install; break;
                    }
                    return true;
                }
                if (event == Event::Return) {
                    if (state->inputs.on_save_globals)
                        state->inputs.on_save_globals(state->inputs.globals);
                    return true;
                }
                return false;
            }

            // --- Plugins tab ---
            case SettingsTab::Plugins: {
                const int n = (int)state->inputs.per_plugin.size();
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    state->plugin_selected = std::max(0, state->plugin_selected - 1);
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    state->plugin_selected = std::min(std::max(0, n - 1),
                                                      state->plugin_selected + 1);
                    return true;
                }
                if (n == 0) return false;
                auto& p = state->inputs.per_plugin[state->plugin_selected];
                state->focused_plugin_index = (std::size_t)state->plugin_selected;

                if (event == Event::Character(' ')) {
                    p.enabled = !p.enabled;
                    if (state->inputs.on_save_plugin)
                        state->inputs.on_save_plugin(p.plugin_id, p);
                    return true;
                }
                if (event == Event::Character('u') || event == Event::Character('U')) {
                    p.auto_update = !p.auto_update;
                    return true;
                }
                if (event == Event::ArrowRight) {
                    state->active_tab = SettingsTab::Options;
                    return true;
                }
                return false;
            }

            // --- Options tab ---
            case SettingsTab::Options: {
                if (event == Event::ArrowLeft) {
                    state->active_tab = SettingsTab::Plugins;
                    return true;
                }
                if (!state->focused_plugin_index) return false;
                auto& p = state->inputs.per_plugin[*state->focused_plugin_index];
                const int n = (int)p.options.size();
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    state->option_selected = std::max(0, state->option_selected - 1);
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    state->option_selected = std::min(std::max(0, n - 1),
                                                      state->option_selected + 1);
                    return true;
                }
                return false;
            }

            // --- Keybindings tab ---
            case SettingsTab::Keybindings: {
                if (event == Event::ArrowLeft) {
                    state->active_tab = SettingsTab::Plugins;
                    return true;
                }
                if (!state->focused_plugin_index) return false;
                auto& p = state->inputs.per_plugin[*state->focused_plugin_index];

                // Adding new keybinding
                if (state->kb_adding) {
                    if (event == Event::Return) {
                        if (!state->kb_new_name.empty() && !state->kb_new_hotkey.empty()) {
                            KeybindingEntry kb{
                                .name = state->kb_new_name,
                                .hotkey = state->kb_new_hotkey,
                                .scope = "global",
                                .enabled = true,
                            };
                            p.keybindings.push_back(kb);
                            if (state->inputs.on_update_keybinding)
                                state->inputs.on_update_keybinding(p.plugin_id, kb);
                            state->kb_adding = false;
                            state->kb_new_name.clear();
                            state->kb_new_hotkey.clear();
                        }
                        return true;
                    }
                    // Switch field: Tab
                    if (event == Event::Tab) {
                        state->kb_edit_field = 1 - state->kb_edit_field;
                        return true;
                    }
                    // Type into the active field
                    if (event.is_character()) {
                        if (state->kb_edit_field == 0)
                            state->kb_new_name.push_back(event.character()[0]);
                        else
                            state->kb_new_hotkey.push_back(event.character()[0]);
                        return true;
                    }
                    if (event == Event::Backspace) {
                        if (state->kb_edit_field == 0 && !state->kb_new_name.empty())
                            state->kb_new_name.pop_back();
                        else if (state->kb_edit_field == 1 && !state->kb_new_hotkey.empty())
                            state->kb_new_hotkey.pop_back();
                        return true;
                    }
                    return false;
                }

                // Navigate existing list
                const int n = (int)p.keybindings.size();
                if (event == Event::ArrowUp || event == Event::Character('k')) {
                    state->keybinding_selected =
                        std::max(0, state->keybinding_selected - 1);
                    return true;
                }
                if (event == Event::ArrowDown || event == Event::Character('j')) {
                    state->keybinding_selected =
                        std::min(std::max(0, n - 1),
                                 state->keybinding_selected + 1);
                    return true;
                }
                // Add
                if (event == Event::Character('a') || event == Event::Character('A')) {
                    state->kb_adding = true;
                    state->kb_edit_field = 0;
                    state->kb_new_name.clear();
                    state->kb_new_hotkey.clear();
                    return true;
                }
                if (n == 0) return false;
                auto& kb = p.keybindings[state->keybinding_selected];
                // Toggle enabled
                if (event == Event::Character(' ')) {
                    kb.enabled = !kb.enabled;
                    if (state->inputs.on_update_keybinding)
                        state->inputs.on_update_keybinding(p.plugin_id, kb);
                    return true;
                }
                // Remove
                if (event == Event::Character('r') || event == Event::Character('R')) {
                    const std::string name = kb.name;
                    p.keybindings.erase(p.keybindings.begin() + state->keybinding_selected);
                    state->keybinding_selected = std::max(
                        0, state->keybinding_selected - 1);
                    if (state->inputs.on_remove_keybinding)
                        state->inputs.on_remove_keybinding(p.plugin_id, name);
                    return true;
                }
                return false;
            }
        }
        return false;
    });
}

} // namespace loom::ui::plugins::plugin_settings_dialog
