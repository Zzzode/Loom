// app_dialog_registration.cppm — thin module interface for dialog renderer
// registration.
//
// Exposes four register_*_dialog_renderers() declarations and imports only the
// lightweight cc.ui.dialogs.system (needed to name the DialogRendererRegistry
// parameter type). The heavy dialog-renderer fan-out lives in four sibling
// module implementation units (app_dialog_registration_{default,modal,bottom,
// all}.cpp), one per renderer aggregator, so that NO single translation unit
// imports more than one aggregator's closure. Importing all four aggregators in
// one TU (their closures are disjoint: 6+5+11+22 = 44 dialog implementations)
// crashes Clang's codegen; splitting them keeps each TU small and keeps the
// entire dialog closure out of app.cppm's source-location budget. See the
// impl units for the full rationale.
export module loom.ui.app.app_dialog_registration;

import loom.ui.dialogs.system;

export namespace cc::ui::app_dialogs {

void register_default_dialog_renderers(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);
void register_modal_dialog_renderers(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);
void register_bottom_dialog_renderers(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);
void register_all_dialog_renderers(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);
void register_hooks_dialog_renderer(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);
void register_teams_dialog_renderer(
    cc::ui::dialogs::system::DialogRendererRegistry& registry);

/// RFC 0002 F2 row 6: register the feature-dialog factories (agent wizard,
/// plugin install wizard, plugin trust dialog) into the
/// cc.ui.foundation.feature_dialog_protocol erased-factory registry. The
/// concrete static_pointer_cast of the erased request lives in the impl unit
/// (app_feature_dialog_registration.cpp), never in a feature or dialogs
/// module. Called once at app startup, alongside the dialog renderers.
void register_feature_dialog_factories();

}  // namespace cc::ui::app_dialogs
