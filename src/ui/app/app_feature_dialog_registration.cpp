// app_feature_dialog_registration.cpp — impl unit for
// loom.ui.app.app_dialog_registration.
//
// RFC 0002 F2 (row 6 registry inversion): registers the concrete dialog
// factories for the feature-owned views (agent wizard, plugin install
// wizard, plugin trust dialog) into the
// loom.ui.foundation.feature_dialog_protocol erased-factory registry. This is
// the composition root: the static_pointer_cast of the erased
// shared_ptr<void> request back to the leaf's concrete request type lives
// ONLY here (grep src/ to verify — no feature or dialogs module casts the
// erased request).
//
// The feature modules (agent_wizard, plugin_install_flow) build a neutral
// request and resolve the factory by ViewKind; they never import the
// dialogs area. The dialogs-side generic adapter (feature_wizard_adapter)
// builds the wizard from the neutral request; the trust factory below maps
// the neutral trust request to a td::TrustDialogProps and the neutral
// TrustChoice back to td::TrustChoice.
//
// This TU imports the dialogs area (app -> dialogs is downward-legal, app
// rank 11 > dialogs rank 9) and the protocol leaf (app -> foundation,
// downward). It is a sibling impl unit of app_dialog_registration_default.cpp
// so its trust_dialog + wizard_adapter closure stays out of the other
// registration TUs.
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

module loom.ui.app.app_dialog_registration;

import std;

import loom.ui.foundation.feature_dialog_protocol;
import loom.ui.dialogs.feature_wizard_adapter;
import loom.ui.dialogs.trust_dialog;

namespace loom::ui::app_dialogs {

namespace fdp = loom::ui::feature_dialog_protocol;
namespace fwa = loom::ui::feature_wizard_adapter;
namespace td = loom::ui::trust_dialog;

namespace {

/// Map the dialogs-area TrustChoice to the protocol leaf's neutral
/// TrustChoice (1:1 — the leaf enum mirrors td::TrustChoice).
fdp::TrustChoice to_neutral_trust_choice(td::TrustChoice choice) {
    switch (choice) {
        case td::TrustChoice::AllowOnce:    return fdp::TrustChoice::AllowOnce;
        case td::TrustChoice::AlwaysAllow:  return fdp::TrustChoice::AlwaysAllow;
        case td::TrustChoice::ViewFile:     return fdp::TrustChoice::ViewFile;
        case td::TrustChoice::EnableAnyway: return fdp::TrustChoice::EnableAnyway;
        case td::TrustChoice::Cancel:       return fdp::TrustChoice::Cancel;
    }
    return fdp::TrustChoice::Cancel;
}

/// Build a wizard factory for a ViewKind whose erased request is a
/// FeatureWizardRequest (AgentWizard, PluginInstall). The concrete cast is
/// confined to this composition-root TU.
fdp::DialogFactory make_wizard_factory() {
    return [](std::shared_ptr<void> erased) -> ftxui::Component {
        auto request =
            std::static_pointer_cast<fdp::FeatureWizardRequest>(erased);
        if (!request) return nullptr;
        return fwa::MakeFeatureWizard(*request);
    };
}

/// Build the plugin-trust factory: expand the neutral FeatureTrustRequest
/// into a full td::TrustDialogProps and map the choice back.
fdp::DialogFactory make_trust_factory() {
    return [](std::shared_ptr<void> erased) -> ftxui::Component {
        auto request =
            std::static_pointer_cast<fdp::FeatureTrustRequest>(erased);
        if (!request) return nullptr;
        td::TrustDialogProps props;
        props.on_done = [cb = std::move(request->on_done)](td::TrustChoice choice) {
            if (cb) cb(to_neutral_trust_choice(choice));
        };
        props.action = td::ActionType::PluginInstall;
        props.action_label = "Plugin Installation";
        props.marketplace_domain = std::move(request->marketplace_domain);
        props.plugin_has_signature = request->has_signature;
        return td::MakeTrustDialogComponent(std::move(props));
    };
}

}  // namespace

void register_feature_dialog_factories() {
    fdp::register_dialog_factory(fdp::ViewKind::AgentWizard,
                                 make_wizard_factory());
    fdp::register_dialog_factory(fdp::ViewKind::PluginInstall,
                                 make_wizard_factory());
    fdp::register_dialog_factory(fdp::ViewKind::PluginTrust,
                                 make_trust_factory());
}

}  // namespace loom::ui::app_dialogs
