// permission_store.cppm — RFC 0002 F3 store: permission-prompt state
// (tool kind + ToolUseConfirm subset: bash / file edit-write-read / web /
// skill payloads), sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). This store's field types are std + types.types
// primitives only, so it imports no loom.ui.* area at all.
module;

export module loom.ui.screens.permission_store;

import std;

export namespace loom::ui::repl_screen {

/// Tool kind for permission prompt dispatch.
/// Determines which faithful permission panel renderer to use.
/// Moved here from repl_state.cppm in RFC 0002 F3 (PermissionStore shard).
enum class PermissionToolKind : std::uint8_t {
    Generic,        ///< Fallback — old simple dialog
    Bash,           ///< Bash / shell command
    FileEdit,       ///< File edit (with diff)
    FileWrite,      ///< File write / create
    FileRead,       ///< File read
    Glob,           ///< Glob search
    Grep,           ///< Grep search
    WebFetch,       ///< Web fetch
    WebSearch,      ///< Web search
    Skill,          ///< Skill execution
    PlanMode,       ///< Enter/exit plan mode
    MCP,            ///< MCP tool
    LSP,            ///< LSP tool
    Agent,          ///< Agent spawn
    NotebookEdit,   ///< Notebook cell edit
    _COUNT,
};

/// Permission prompt subset (TS ToolUseConfirm).  Full shape: UI8/UI9.
/// Moved here from repl_state.cppm in RFC 0002 F3 (PermissionStore shard).
struct PermissionRequestInfo {
    std::string tool_name, description;
    std::optional<std::string> file_path;
    std::vector<std::string> risk_labels;
    bool can_always_allow = true;

    // -- Tool-specific payload (populated only for matching tool_kind) --

    // Bash
    std::optional<std::string> bash_command;
    std::optional<std::string> bash_working_dir;
    bool bash_is_destructive = false;
    std::string bash_destructive_reason;

    // FileEdit / FileWrite / FileRead (shared path fields)
    std::optional<std::string> file_old_content;    // edit: old_string / write: old_content
    std::optional<std::string> file_new_content;    // edit: new_string / write: content
    bool file_replace_all = false;                  // edit only
    bool file_exists = false;                       // write only (overwrite vs create)
    std::optional<std::string> file_language;       // syntax highlight hint
    std::optional<std::string> file_relative_path;
    std::optional<std::string> file_filename;       // basename

    // Web
    std::optional<std::string> web_url;

    // Skill
    std::optional<std::string> skill_name;
    std::optional<std::string> skill_source;  // "bundled" / "user" / "plugin"
};

/// RFC 0002 F3 store — permission-prompt state (tool kind + ToolUseConfirm
/// subset), sharded out of ReplScreenState. Homed in loom.ui.screens
/// (rank 10): the field types are std + types.types primitives only, so no
/// cross-area edge is created at all. UI-thread-affined plain data —
/// see the file header for the threading and import rules.
struct PermissionStore {
    /// The active permission request, when a tool is blocked awaiting a
    /// user decision. nullopt = no pending permission prompt. Set by the
    /// engine when a tool requires approval; reset on allow/deny/abort.
    std::optional<PermissionRequestInfo> permission_request;
};

}  // namespace loom::ui
