module;
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
module loom.ui.app.app_dialog_registration;
import std;
import loom.ui.dialogs.system;

namespace loom::ui::app_dialogs {
void register_agent_instances_dialog_renderer(
    loom::ui::dialogs::system::DialogRendererRegistry& registry) {
    namespace ds = loom::ui::dialogs::system;
    registry.register_dialog(
        ds::DialogType::AgentInstances,
        [](ds::DialogPayloadVariant& payload, const ds::DialogRenderContext& ctx) {
            auto* p = std::get_if<ds::AgentInstancesPayload>(&payload);
            if (!p || !p->component) return ftxui::text("");
            if (p->on_resize) p->on_resize(ctx.term_cols, ctx.modal_available_rows);
            return (*std::static_pointer_cast<ftxui::Component>(p->component))->Render();
        },
        [](ds::DialogPayloadVariant& payload, const ftxui::Event& event) {
            auto* p = std::get_if<ds::AgentInstancesPayload>(&payload);
            if (!p || !p->component) return false;
            // Preserve the REPL's global interrupt, exit and redraw handlers.
            if (event == ftxui::Event::Character('\x03') ||
                event == ftxui::Event::Character('\x04') ||
                event == ftxui::Event::Character('\x0c'))
                return false;
            // Empty lists still own Enter and ordinary input.
            (void)(*std::static_pointer_cast<ftxui::Component>(p->component))->OnEvent(event);
            return true;
        });
}
}  // namespace loom::ui::app_dialogs
