/// @file feature_wizard_adapter.cppm
/// @brief Generic adapter: the feature_dialog_protocol neutral wizard request
/// -> cc.ui.dialogs.wizard_dialog's WizardComponent (RFC 0002 F2, row 6).
///
/// MODULE:   cc.ui.dialogs.feature_wizard_adapter
/// AREA:     dialogs (UI9 rank 9).
/// IMPORTS:  cc.ui.foundation.feature_dialog_protocol (rank 2 — downward),
///           cc.ui.dialogs.wizard_dialog (same area). NO features import:
///           this adapter is generic and works for any ViewKind whose erased
///           request is a FeatureWizardRequest (AgentWizard, PluginInstall).
///
/// This is the dialogs-side half of the features -> dialogs inversion: the
/// feature modules no longer import wizard_dialog; they build a neutral
/// FeatureWizardRequest and resolve a factory by ViewKind. The composition
/// root (src/ui/app/*) static_pointer_cast's the erased request back to a
/// FeatureWizardRequest and calls MakeFeatureWizard below.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module cc.ui.dialogs.feature_wizard_adapter;

import std;

import cc.ui.foundation.feature_dialog_protocol;
import cc.ui.dialogs.wizard_dialog;

export namespace cc::ui::feature_wizard_adapter {

namespace fdp = cc::ui::feature_dialog_protocol;
namespace wd = cc::ui::wizard_dialog;

/// Build a wizard component from a neutral feature wizard request.
/// Generic — works for any ViewKind whose erased request is a
/// FeatureWizardRequest (the composition root selects the request type).
[[nodiscard]] inline ftxui::Component MakeFeatureWizard(
    const fdp::FeatureWizardRequest& request) {
    wd::WizardProviderProps props;
    props.title = request.title;
    props.show_step_counter = request.show_step_counter;
    props.on_cancel = request.on_cancel;
    props.on_complete = request.on_complete;
    props.steps.reserve(request.steps.size());
    for (const auto& s : request.steps) {
        wd::WizardStep step;
        step.id = s.id;
        step.title = s.title;
        step.description = s.description;
        step.create_content = s.create_content;
        props.steps.push_back(std::move(step));
    }
    return wd::WizardComponent(std::move(props));
}

}  // namespace cc::ui::feature_wizard_adapter
