// repl_screen_dialog_panels.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). the settings / trust lazy dialog components.
//
// Phase A (#184957): textual FTXUI GMF headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.dialogs.settings_dialog;
import loom.config.config;
import loom.ui.dialogs.trust_dialog;
import loom.ui.prompt.prompt_input_footer;  // c22: footer notification queue

namespace loom::ui::repl_screen {
using namespace ftxui;

namespace dialog_router {

// -------------------------------------------------------------------
// Settings dialog helpers (UI3)
// -------------------------------------------------------------------

namespace settings_ns = loom::ui::dialogs::settings_dialog;

using settings_ns::CommandResultDisplay;
using settings_ns::SettingsDialogOptions;
using settings_ns::SettingsTabId;
using settings_ns::MakeSettingsDialog;

/// Lazily create (or re-create) the settings dialog component.
/// If `state->dialog_store.settings_config` is non-null it is used for reads/writes,
/// otherwise a fresh internal ConfigManager is used (snapshot only).
[[nodiscard]] std::shared_ptr<Component> get_settings_component(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb) {
    if (!s->dialog_store.settings_component) {
        // Use the supplied config manager, otherwise manufacture a default.
        static thread_local loom::core::ConfigManager fallback_config;
        loom::core::ConfigManager* cfg = s->dialog_store.settings_config
                                            ? static_cast<loom::core::ConfigManager*>(
                                                  s->dialog_store.settings_config)
                                            : &fallback_config;
        if (cfg == &fallback_config) {
            // c23: the fallback is session-scoped (thread_local static) and
            // would otherwise serve a stale snapshot after an external edit.
            // Reload before each dialog open — load() is idempotent and
            // tolerates missing tiers. Quiet so a soft-tier parse warning
            // reaches the toast sink below instead of stderr.
            (void)fallback_config.load(loom::core::LoadOptions{.quiet = true});
            // c22: surface soft-tier (user/local) parse warnings as toasts.
            // QueueAddNotification dedups by key, so a repeated open does
            // not stack duplicates.
            namespace pif = loom::ui::prompt::footer;
            for (auto& diag : fallback_config.drain_load_diagnostics()) {
                pif::NotificationItem item;
                item.key = "config-tier-warning:" + diag.path;
                item.text = diag.message;
                item.color = "warning";
                item.priority = pif::NotificationPriority::High;
                item.timeout_ms = 8000;
                pif::QueueAddNotification(s->footer_notification_queue, item);
            }
        }
        SettingsDialogOptions opts;
        opts.initial_tab = static_cast<SettingsTabId>(s->dialog_store.settings_initial_tab);
        opts.on_close = [s, cb](std::optional<std::string>, CommandResultDisplay) {
            s->mode = ReplMode::Normal;
            s->dialog_store.settings_component.reset();
            s->dialog_store.settings_initial_tab = 0;  // reset to General for next open
            if (cb->on_mode_change) cb->on_mode_change(ReplMode::Normal);
        };
        s->dialog_store.settings_component = std::make_shared<Component>(
            MakeSettingsDialog(*cfg, std::move(opts)));
    }
    return std::static_pointer_cast<Component>(s->dialog_store.settings_component);
}

/// Reset (destroy) the settings component so the next entry starts fresh
/// (empty dirty flag, pristine snapshot, tab=General).
void reset_settings_component(const std::shared_ptr<ReplScreenState>& s) {
    s->dialog_store.settings_component.reset();
    s->dialog_store.settings_initial_tab = 0;  // General
}

/// Render the settings dialog content as an Element.
[[nodiscard]] Element render_settings(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb) {
    auto comp = get_settings_component(s, cb);
    return comp ? (*comp)->Render() : text("");
}

/// Forward an event to the settings dialog component.
bool forward_settings(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev) {
    auto comp = get_settings_component(s, cb);
    return comp && (*comp)->OnEvent(std::move(ev));
}

// -------------------------------------------------------------------
// Trust dialog helpers (UI8)
// -------------------------------------------------------------------
//
// Trust dialog lives in the STANDALONE slot (full-takeover).  We also
// keep a ReplMode-driven path as a bridge for callers that haven't
// migrated to the queue yet.  The Component is lazy-created via
// MakeWorkspaceTrustDialog() and stored as an opaque handle so
// ReplScreenState doesn't need to import the trust_dialog types.

namespace trust_ns = loom::ui::trust_dialog;
using trust_ns::TrustChoice;
using trust_ns::WorkspaceTrustProps;
using trust_ns::SecuritySources;
using trust_ns::MakeWorkspaceTrustDialog;

/// Lazily create (or re-create) the trust dialog component.
[[nodiscard]] std::shared_ptr<Component> get_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb) {
    if (!s->dialog_store.wizard_trust) {
        WorkspaceTrustProps props;
        props.workspace_path = s->cwd.empty() ? "." : s->cwd;
        props.on_done = [s, cb](TrustChoice choice) {
            // Bridge choices onto the permission + exit callbacks.  The
            // enum actually ships 5 values (AllowOnce/AlwaysAllow/ViewFile/
            // Cancel/EnableAnyway) — the "Exit" variant is handled by the
            // on_exit callback directly inside the trust dialog component
            // when the user chooses the corresponding option, not via
            // choice enum here.
            using C = TrustChoice;
            if (cb->on_permission_response) {
                switch (choice) {
                  case C::AllowOnce:    cb->on_permission_response(true, false); break;
                  case C::AlwaysAllow:  cb->on_permission_response(true, true);  break;
                  case C::EnableAnyway: cb->on_permission_response(true, true);  break;
                  case C::ViewFile:     cb->on_permission_response(true, false); break;
                  case C::Cancel:
                  default:              cb->on_permission_response(false, std::nullopt); break;
                }
            }
            s->mode = ReplMode::Normal;
            if (cb->on_mode_change) cb->on_mode_change(ReplMode::Normal);
            s->dialog_store.wizard_trust.reset();
        };
        s->dialog_store.wizard_trust = std::make_shared<Component>(
            MakeWorkspaceTrustDialog(std::move(props)));
    }
    return std::static_pointer_cast<Component>(s->dialog_store.wizard_trust);
}

/// Render the trust dialog content as an Element.
[[nodiscard]] Element render_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb) {
    auto d = get_trust_dialog(s, cb);
    return d ? (*d)->Render() : text("(trust dialog unavailable)");
}

/// Forward an event to the trust dialog component.
bool forward_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev) {
    auto d = get_trust_dialog(s, cb);
    return d && (*d)->OnEvent(std::move(ev));
}

}  // namespace dialog_router

}  // namespace loom::ui::repl_screen
