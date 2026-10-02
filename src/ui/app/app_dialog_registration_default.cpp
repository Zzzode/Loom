// app_dialog_registration_default.cpp — impl unit for loom.ui.app_dialog_registration.
// Imports loom.ui.dialogs.default_renderers (6 core dialogs) and
// loom.ui.dialogs.plugin_dialog_renderer (thin interface — just a function
// declaration) so this TU's closure stays small.  The heavy plugin_dialog +
// plugin_ui_data + plugin_marketplace imports live in the renderer's
// implementation unit (plugin_dialog_renderer_impl.cpp), which has its own
// independent source-location budget.
//
// RFC 0002 F2 (row 4): the Doctor dialog renderer is registered from the
// screens side (loom.ui.screens.doctor_dialog_registration) so that the dialogs
// area no longer imports screens (dialogs -> screens was a UI9 back edge).
// app -> screens is downward-legal (app rank 11 > screens rank 10).
module loom.ui.app.app_dialog_registration;

import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.dialogs.plugin_dialog_renderer;
import loom.ui.screens.doctor_dialog_registration;
import loom.ui.screens.statusline_dialog;

namespace loom::ui::app_dialogs {
void register_default_dialog_renderers(
    loom::ui::dialogs::system::DialogRendererRegistry& registry) {
    loom::ui::dialogs::default_renderers::register_default_renderers(registry);
    loom::ui::dialogs::plugin_dialog_renderer::register_plugin_dialog_renderer(
        registry);
    // Doctor renderer — registered from the screens side (RFC 0002 F2 row 4).
    loom::ui::screens::doctor_dialog_registration::register_doctor_renderer(
        registry);
    // Statusline segment toggle — same screens-side registration pattern.
    loom::ui::screens::statusline_dialog::register_statusline_dialog_renderer(
        registry);
}
}  // namespace loom::ui::app_dialogs
