// mcp_status_store.cppm — RFC 0002 F3 store: MCP integration status
// (AT-09 inbound IDE at_mentioned inserts), sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads.
// The mutex that protected the staged at-mention queue in repl_state.cppm
// (pending_at_mention_mutex) and the staging queue itself move OUT to the
// AppImpl composition layer: the MCP receive thread stages tokens there
// under mutex, and the UI thread drains them into this store
// (AppAdapter::DrainPendingAtMentionInserts) before applying them to the
// prompt (ApplyPendingAtMentionInserts). This store therefore holds only
// DRAINED data — the worker thread never touches it. Cross-store reads go
// through selectors wired by the composition root, never a direct field
// reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). This store's field type is std::vector<std::string>,
// so it imports no loom.ui.* area at all.
module;

export module loom.ui.screens.mcp_status_store;

import std;

export namespace loom::ui::repl_screen {

/// RFC 0002 F3 store — MCP integration status, sharded out of
/// ReplScreenState. Homed in loom.ui.screens (rank 10): the field type is a
/// std-only primitive, so no cross-area edge is created at all.
/// UI-thread-affined plain data — see the file header for the threading
/// and import rules.
struct McpStatusStore {
    /// AT-09: inbound IDE at_mentioned tokens ("@<relpath>#L<a>-<b>") that
    /// have been DRAINED from the AppImpl staging queue on the UI thread
    /// and are awaiting application to the prompt input. The MCP receive
    /// thread never touches this vector — it stages into AppImpl's
    /// mutex-protected queue; the UI thread drains
    /// (DrainPendingAtMentionInserts) and then applies
    /// (ApplyPendingAtMentionInserts). Empty in steady state.
    /// Inserts at the cursor.
    std::vector<std::string> pending_at_mention_inserts;
};

}  // namespace loom::ui
