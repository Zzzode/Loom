/// @file feature_dialog_protocol.cppm
/// @brief Neutral vocabulary + erased factory registry for feature-owned
/// dialogs (RFC 0002 F2, row 6 registry inversion).
///
/// MODULE:   loom.ui.foundation.feature_dialog_protocol
/// AREA:     foundation (UI9 rank 2 — below both features(8) and dialogs(9)).
/// IMPORTS:  std + textual FTXUI only. NO loom.* module imports, by design:
///           this leaf is the seam that lets the features area obtain dialog
///           components without importing the dialogs area (features ->
///           dialogs was the last UI9 back edge).
///
/// The inversion: a feature module (agent_wizard, plugin_install_flow) builds
/// a NEUTRAL request (the types below) and resolves a factory by ViewKind.
/// The composition root (src/ui/app/*) registers the concrete factory that
/// static_pointer_cast's the erased request back to the leaf type and builds
/// the real dialog. The concrete cast lives ONLY in src/ui/app/* — grep the
/// tree to verify.
///
/// ViewKind key set (the string/shape coupling between the feature-side
/// resolve and the composition-root registration — catalogued in
/// docs/decisions/design-decisions.md):
///   AgentWizard   — the 4-step agent create/edit wizard
///   PluginInstall — the 5-step plugin install wizard
///   PluginTrust   — the plugin trust-validation dialog (wizard step 3)
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

export module loom.ui.foundation.feature_dialog_protocol;

import std;

export namespace loom::ui::feature_dialog_protocol {

using ftxui::Component;

/// Identifies a feature-owned dialog view. The composition root registers a
/// factory for each kind; feature modules resolve the factory by kind and
/// pass an erased request (shared_ptr<void>).
enum class ViewKind : std::uint8_t {
    AgentWizard,
    PluginInstall,
    PluginTrust,
};

/// One step of a feature wizard. Neutral — names no dialogs-area type.
/// Mirrors the agent_wizard AgentWizardStepEntry shape (moved here).
struct FeatureWizardStep {
    std::string id;
    std::string title;
    std::string description;
    std::function<Component()> create_content;
};

/// A neutral wizard request: everything the dialogs-side generic wizard
/// adapter needs to build a wizard, with no dialogs-area types named.
/// Mirrors the agent_wizard WizardProviderProps shape (moved here).
struct FeatureWizardRequest {
    std::string title;
    bool show_step_counter = true;
    std::function<void()> on_cancel;
    std::function<void()> on_complete;
    std::vector<FeatureWizardStep> steps;
};

/// Neutral trust decision. Mirrors loom.ui.dialogs.trust_dialog::TrustChoice
/// (== loom.ui.dialogs.trust_utils::TrustChoice) without naming it, so the
/// features area never imports trust_dialog. The composition root maps
/// td::TrustChoice <-> this enum 1:1.
enum class TrustChoice : std::uint8_t {
    AllowOnce,      // Allow this single action
    AlwaysAllow,    // Persist: skip prompt for this path/plugin
    ViewFile,       // Show file contents before deciding (High tier only)
    EnableAnyway,   // Critical tier: user typed YES to override
    Cancel,         // Block / do nothing
};

/// A neutral trust-dialog request: the trust data + a neutral on_done.
/// Carries only what the plugin-install wizard step 3 needs; the composition
/// root expands it into a full td::TrustDialogProps.
struct FeatureTrustRequest {
    std::string marketplace_domain;
    bool has_signature = true;
    std::function<void(TrustChoice)> on_done;
};

/// Erased dialog factory: takes a type-erased request (shared_ptr<void>),
/// returns a Component. The concrete static_pointer_cast happens ONLY in the
/// composition root (src/ui/app/*) — never in a feature or dialogs module.
using DialogFactory = std::function<Component(std::shared_ptr<void>)>;

namespace detail {

/// Registry storage. Function-local static in an inline function: one
/// definition across all importers, initialized on first use (registration
/// happens at app construction, before any feature resolves a factory).
inline std::unordered_map<ViewKind, DialogFactory>& registry() {
    static std::unordered_map<ViewKind, DialogFactory> factories;
    return factories;
}

}  // namespace detail

/// Register a factory for a view kind. Called once at startup from the
/// composition root. Re-registering a kind replaces its factory.
inline void register_dialog_factory(ViewKind kind, DialogFactory factory) {
    detail::registry()[kind] = std::move(factory);
}

/// Resolve the factory for a view kind. Returns nullptr if unregistered.
[[nodiscard]] inline DialogFactory resolve_dialog_factory(ViewKind kind) {
    auto it = detail::registry().find(kind);
    return it != detail::registry().end() ? it->second : nullptr;
}

}  // namespace loom::ui::feature_dialog_protocol
