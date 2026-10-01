/// @file doctor_dialog_registration.cppm
/// @brief Registers the Doctor dialog renderer into DialogRendererRegistry.
///
/// MODULE:   cc.ui.screens.doctor_dialog_registration
/// LICENCE:  Exported.  Imported by the app composition root to register the
///           Doctor dialog renderer alongside the default set.
///
/// RFC 0002 F2 (row 4 registry inversion): the doctor renderer registration
/// moved OUT of cc.ui.dialogs.default_renderers so that the dialogs area no
/// longer imports the screens area.  dialogs -> screens was a back edge under
/// the UI9 rank table (screens rank 10 > dialogs rank 9); screens -> dialogs
/// is downward-legal, so the registration lives here on the screens side.
/// The composition root calls register_doctor_renderer() next to
/// register_default_renderers().
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.screens.doctor_dialog_registration;

import std;

import loom.ui.dialogs.system;
import loom.ui.screens.doctor_screen;

export namespace cc::ui::screens::doctor_dialog_registration {

namespace dsys = cc::ui::dialogs::system;

namespace doctor_detail {
/// Holder component that wraps a DoctorScreen so the dialog system can
/// render it and route events to its CatchEvent handler.  ComponentBase::OnEvent
/// is protected, so this subclass exposes a public InjectEvent() surface.
struct DoctorDialogHolder : public ftxui::ComponentBase {
    ftxui::Component screen;

    explicit DoctorDialogHolder(ftxui::Component s) : screen(std::move(s)) {
        ComponentBase::Add(screen);
    }

    ftxui::Element Render() override { return screen->Render(); }

    bool InjectEvent(const ftxui::Event& event) {
        return ComponentBase::OnEvent(event);
    }
};
} // namespace doctor_detail

/// Register the Doctor dialog renderer into a registry.
/// Call this once at app startup, alongside register_default_renderers().
void register_doctor_renderer(dsys::DialogRendererRegistry& registry) {
    // ─── Doctor — standalone fullscreen diagnostics ────────────────────────────────
    //
    // Wraps the DoctorScreen FTXUI component.  The holder (defined at
    // namespace scope above) routes events through the child hierarchy
    // where DoctorScreen's CatchEvent lives.

    registry.register_dialog(
        dsys::DialogType::Doctor,
        /*renderer=*/
        [](dsys::DialogPayloadVariant& payload,
           const dsys::DialogRenderContext& /*ctx*/) -> ftxui::Element {
            auto* p = std::get_if<dsys::DoctorDialogPayload>(&payload);
            if (!p) return ftxui::text("");

            // Lazily create the DoctorScreen component on first render.
            if (!p->component) {
                using namespace cc::ui::doctor_screen;

                DoctorDataModel model;
                // Populate version info from the live environment.
                auto ctx = default_doctor_context();
                model.version.current_version = ctx.current_version;
                model.version.installation_type = "native";
                model.version.installation_path = "/opt/loom/loom";
                model.version.invoked_binary = "loom";
                // Run all checks so results are ready when the screen opens.
                model.results = RunAllChecks(ctx);

                DoctorScreenOptions opts;
                opts.initial = std::move(model);
                opts.on_done = p->on_done;

                auto screen = DoctorScreen(std::move(opts));
                auto holder = std::make_shared<doctor_detail::DoctorDialogHolder>(
                    std::move(screen));
                p->component = holder;
            }

            auto holder = std::static_pointer_cast<doctor_detail::DoctorDialogHolder>(
                p->component);
            return holder ? holder->Render() : ftxui::text("");
        },
        /*event_handler=*/
        [](dsys::DialogPayloadVariant& payload, const ftxui::Event& event) -> bool {
            auto* p = std::get_if<dsys::DoctorDialogPayload>(&payload);
            if (!p || !p->component) return false;

            auto holder = std::static_pointer_cast<doctor_detail::DoctorDialogHolder>(
                p->component);
            if (!holder) return false;
            return holder->InjectEvent(event);
        }
    );
}

} // namespace cc::ui::screens::doctor_dialog_registration
