/// @file all_renderers.cppm
/// @brief Aggregator import: pulls in every renderer module, then re-exports
///        the union of Render* / Handle*Event functions under a single
///        namespace (loom::ui::dialogs::all_renderers).
///
/// TEST CONTRACT: unit tests alias
///   namespace dr = loom::ui::dialogs::all_renderers;
/// and write dr::RenderXxx(payload, ctx) / dr::HandleXxxEvent(p, e).
/// This module therefore re-exports every such function via explicit
/// `using` declarations so lookup resolves without ambiguous
/// namespace-qualification errors.
module;
export module loom.ui.dialogs.all_renderers;

import std;

import loom.ui.dialogs.default_renderers;
import loom.ui.dialogs.cost_threshold_dialog;
import loom.ui.dialogs.sandbox_permission;
import loom.ui.dialogs.quick_open;

export namespace loom::ui::dialogs::all_renderers {

// ── registration entry-points (kept for historical callers) ──────────────
using loom::ui::dialogs::default_renderers::register_default_renderers;

/// Named registration entry-point matching the module name.
template <typename... Args>
inline void register_all_renderers(Args&&... args) {
    loom::ui::dialogs::default_renderers::register_default_renderers(
        std::forward<Args>(args)...);
}

// ── Layer 1: 6 overlay/bottom dialogs from default_renderers ─────────────
// (ToolPermission, SandboxPermission, PromptDialog, Elicitation,
//  CostThreshold, IdleReturn, GenericDialog fallback)
using loom::ui::dialogs::default_renderers::RenderToolPermission;
using loom::ui::dialogs::default_renderers::HandleToolPermissionEvent;

using loom::ui::dialogs::default_renderers::RenderSandboxPermission;
using loom::ui::dialogs::default_renderers::HandleSandboxPermissionEvent;

using loom::ui::dialogs::default_renderers::RenderPromptDialog;
using loom::ui::dialogs::default_renderers::HandlePromptDialogEvent;

using loom::ui::dialogs::default_renderers::RenderElicitation;
using loom::ui::dialogs::default_renderers::HandleElicitationEvent;

using loom::ui::dialogs::default_renderers::RenderCostThreshold;
using loom::ui::dialogs::default_renderers::HandleCostThresholdEvent;

using loom::ui::dialogs::default_renderers::RenderIdleReturn;
using loom::ui::dialogs::default_renderers::HandleIdleReturnEvent;

using loom::ui::dialogs::default_renderers::RenderGenericDialog;
using loom::ui::dialogs::default_renderers::HandleGenericDialogEvent;

// ── Layer 2: faithful permission-panel modules ───────────────────────────
using loom::ui::dialogs::sandbox_permission::RenderDefault;
using loom::ui::dialogs::sandbox_permission::HandleSandboxPermissionEvent;

// ── Layer 3: quick_open (exact signatures already, direct using) ─────────
using loom::ui::dialogs::quick_open::RenderQuickOpen;
using loom::ui::dialogs::quick_open::HandleQuickOpenEvent;

}  // namespace loom::ui::dialogs::all_renderers
