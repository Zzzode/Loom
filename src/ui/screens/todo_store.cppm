// todo_store.cppm — RFC 0002 F3 store: todo-list panel state, sharded out of
// ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads — the
// TodoWriteTool singleton owns its own mutex; this store is a lean projection
// of it, written by AppAdapter::ProjectTodosToScreenState between frames.
// Cross-store reads go through selectors wired by the composition root, never
// a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). features (8) is downward-legal, so the concrete by-value
// field type (TodoDisplayItem) recreates no up-edge — same pattern as
// task_view_store's LiveTeammate field.
module;

export module loom.ui.screens.todo_store;

import std;

import loom.ui.features.todos.todo_panel;        // TodoDisplayItem

export namespace loom::ui::repl_screen {

/// RFC 0002 F3 store — todo-list panel state, sharded out of ReplScreenState.
/// Homed in loom.ui.screens (rank 10): the concrete cross-area field type
/// (TodoDisplayItem) lives in features (rank 8), so the by-value field
/// recreates no up-edge. Populated by AppAdapter from the TodoWriteTool
/// singleton (loom::tools::todo_write_store). UI-thread-affined plain data —
/// see the file header for the threading and import rules.
struct TodoStore {
    std::vector<todos::TodoDisplayItem> items;
};

}  // namespace loom::ui::repl_screen
