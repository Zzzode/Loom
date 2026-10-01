// app_dialog_registration_bottom.cpp — impl unit for cc.ui.app_dialog_registration.
// Imports ONLY cc.ui.dialogs.bottom_renderers (11 bottom-slot callouts) so this
// TU's closure stays small. See app_dialog_registration.cppm for the rationale.
module loom.ui.app.app_dialog_registration;

import loom.ui.dialogs.system;
import loom.ui.dialogs.bottom_renderers;

namespace loom::ui::app_dialogs {
void register_bottom_dialog_renderers(
    loom::ui::dialogs::system::DialogRendererRegistry& registry) {
    loom::ui::dialogs::bottom_renderers::register_bottom_renderers(registry);
}
}  // namespace loom::ui::app_dialogs
