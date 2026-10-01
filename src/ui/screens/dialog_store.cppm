// dialog_store.cppm — RFC 0002 F3 store: dialog state (overlay dialogs,
// inline panels, wizard/trust component handles, the M7 dialog queue +
// renderer registry), sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). dialogs (9) is downward-legal, so the concrete
// by-value DialogQueue / DialogRendererRegistry fields recreate no
// up-edge.
module;

export module loom.ui.screens.dialog_store;

import std;

import loom.ui.dialogs.system;                     // DialogQueue / DialogRendererRegistry

export namespace loom::ui::repl_screen {

/// RFC 0002 F3 store — dialog state (overlay dialogs, inline panels,
/// wizard/trust component handles, the M7 dialog queue + renderer
/// registry), sharded out of ReplScreenState. Homed in loom.ui.screens
/// (rank 10): the concrete cross-area field types (DialogQueue,
/// DialogRendererRegistry) live in dialogs (rank 9), so by-value fields
/// recreate no up-edge. The component handles are std::shared_ptr<void>
/// (lazy-created dialog components, type-erased so this store need not
/// import their types). UI-thread-affined plain data — see the file
/// header for the threading and import rules.
struct DialogStore {
    // ── M7 Dialog Framework (Task #124) ─────────────────────────────────
    // Dialog queue (4 slots) + renderer/event-handler registry. The
    // engine pushes payloads into dialog_queue between frames; ReplScreen
    // dispatches render + events through dialog_renderers at priority
    // Standalone > Modal > Overlay > Bottom.
    loom::ui::dialogs::system::DialogQueue dialog_queue;
    loom::ui::dialogs::system::DialogRendererRegistry dialog_renderers;

    // ── Overlay dialog component handles (lazy-created, opaque) ─────────
    // UI13: agent wizard component handle (lazily created by
    // dialog_router::get_agent_wizard()). Stored as shared_ptr<void> so
    // the store doesn't need to import the wizard's type.
    std::shared_ptr<void> wizard_agent;
    // UI8: trust dialog component handle (lazy-created; opaque).
    std::shared_ptr<void> wizard_trust;

    // ── Inline panel component handles (lazy-created, opaque) ───────────
    // AgentsView panel component handle (lazy-created; opaque). The agent
    // cards themselves live in TaskViewStore.
    std::shared_ptr<void> agents_component;
    // UI3: settings dialog component (lazy-created). Opaque so the store
    // doesn't need to import the settings dialog module types.
    std::shared_ptr<void> settings_component;
    // Optional external ConfigManager reference. When set, settings_dialog
    // reads/writes against this engine-owned instance; otherwise it uses a
    // thread-local fallback (snapshot-only).
    void* settings_config = nullptr;
    // Initial tab when the settings dialog is opened. Defaults to General;
    // commands like /permissions may set this to Permissions before
    // opening. Typed as int to keep the store free of settings_dialog type
    // deps.
    int settings_initial_tab = 0;  // matches SettingsTabId::General = 0
};

}  // namespace loom::ui
