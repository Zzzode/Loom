// repl_selectors.cppm — cross-store selectors for ReplScreenState.
//
// Selectors encapsulate multi-store reads so consumers (layout, events,
// app) don't reach into individual store fields directly.  Single-store
// predicates (IsToolAnimating, ResetScrollToBottom) live in their
// respective store modules; this module is for reads that span stores
// or need a named abstraction over raw field access.
//
// Homed in loom.ui.screens (rank 10): same area as the stores, so
// no up-edge is created.
module;

export module loom.ui.screens.repl_selectors;

import std;

import loom.ui.screens.dialog_store;       // DialogStore
import loom.ui.screens.task_view_store;   // TaskViewStore / IsToolAnimating

export namespace loom::ui::repl_screen {

/// True when any dialog slot (standalone, modal, overlay, bottom)
/// contains at least one dialog.  Replaces ad-hoc combinations of
/// has_standalone() || has_modal() || has_overlay() at call sites.
[[nodiscard]] inline bool HasAnyDialogOpen(const DialogStore& store) {
    return !store.dialog_queue.empty();
}

/// True when a tool/query is animating (spinner visible).
/// Re-exported from task_view_store for discoverability — the
/// canonical definition lives there.
using loom::ui::repl_screen::IsToolAnimating;

}  // namespace loom::ui
