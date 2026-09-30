// repl_state.cppm — plain-data REPL screen state.
//
// Split from repl_screen.cppm so importers that only need the state struct
// (cc.ui.app.app and its impl shards) do not pull the full rendering closure
// (message/dialog/widget renderers — over a hundred modules) into their BMI.
// repl_screen.cppm re-exports this module, so every existing importer keeps
// seeing these names unchanged.
module;

#include <cstdint>

#include <ftxui/component/event.hpp>

export module cc.ui.screens.repl_state;

import std;

import cc.types.types;
import cc.ui.foundation.ui_types;                 // cc::ui::common::PromptInputMode
import cc.ui.dialogs.system;                     // DialogQueue / payloads
import cc.ui.prompt.prompt_input_footer;         // footer::* projection types
// RFC 0002 F3 shim: re-exports the MessagesStore shard so importers keep
// seeing MessageDisplayEntry during the per-store migration. Deleted in F3
// Finalize — importers then import cc.ui.screens.messages_store directly.
export import cc.ui.screens.messages_store;      // MessagesStore / MessageDisplayEntry
// RFC 0002 F3 shim: re-exports the PromptStore shard so importers keep
// seeing StashedPrompt during the per-store migration. Deleted in F3
// Finalize — importers then import cc.ui.screens.prompt_store directly.
export import cc.ui.screens.prompt_store;        // PromptStore / StashedPrompt
// RFC 0002 F3 shim: re-exports the TaskViewStore shard so importers keep
// seeing SpinnerMode during the per-store migration. Deleted in F3
// Finalize — importers then import cc.ui.screens.task_view_store directly.
export import cc.ui.screens.task_view_store;     // TaskViewStore / SpinnerMode
// RFC 0002 F3 shim: re-exports the PermissionStore shard so importers keep
// seeing PermissionRequestInfo / PermissionToolKind during the per-store
// migration. Deleted in F3 Finalize — importers then import
// cc.ui.screens.permission_store directly.
export import cc.ui.screens.permission_store;    // PermissionStore / PermissionRequestInfo
// RFC 0002 F3 shim: re-exports the DialogStore shard so importers keep
// seeing the dialog component handles / DialogQueue during the per-store
// migration. Deleted in F3 Finalize — importers then import
// cc.ui.screens.dialog_store directly.
export import cc.ui.screens.dialog_store;        // DialogStore / dialog handles
// RFC 0002 F3 shim: re-exports the McpStatusStore shard so importers keep
// seeing McpStatusStore during the per-store migration. Deleted in F3
// Finalize — importers then import cc.ui.screens.mcp_status_store directly.
export import cc.ui.screens.mcp_status_store;    // McpStatusStore
import cc.ui.features.agents.agent_cards;        // AgentCardData
import cc.ui.features.agents.agent_wizard;       // WizardDraft (callback sig)
import cc.ui.visual.markdown;                    // StreamingMarkdown (ptr field)

export namespace cc::ui::repl_screen {
using namespace ftxui;

// M7: pull the dialog framework types up into convenient aliases.
namespace dsys_fw   = cc::ui::dialogs::system;
namespace cards     = cc::ui::agents::cards;
using DialogQueue            = dsys_fw::DialogQueue;
using DialogRendererRegistry = dsys_fw::DialogRendererRegistry;
using DialogRenderContext    = dsys_fw::DialogRenderContext;
using DialogPayloadVariant   = dsys_fw::DialogPayloadVariant;

// =========================================================
// Enums
// =========================================================

/// Top-level mode.  1:1 with TS focusedInputDialog union (REPL.tsx:2017)
/// plus contextual panel modes.  TS names in trailing comments.
enum class ReplMode : std::uint8_t {
    Normal,                   // (none) — base layout
    // Dialogs (overlay via dbox)
    MessageSelector,          // 'message-selector'
    SandboxPermission,        // 'sandbox-permission'
    ToolPermission,           // 'tool-permission'
    PromptHook,               // 'prompt' (was: HookPrompt)
    WorkerSandboxPermission,  // 'worker-sandbox-permission'
    Elicitation,              // 'elicitation'
    CostThreshold,            // 'cost'
    IdleReturn,               // 'idle-return'
    UltraplanChoice,          // 'ultraplan-choice'  (ULTRAPLAN)
    UltraplanLaunch,          // 'ultraplan-launch'  (ULTRAPLAN)
    IdeOnboarding,            // 'ide-onboarding'
    InitOnboarding,           // 'init-onboarding'
    ModelSwitch,              // 'model-switch'        (ant-only)
    UndercoverCallout,        // 'undercover-callout'  (ant-only)
    EffortCallout,            // 'effort-callout'
    RemoteCallout,            // 'remote-callout'
    DesktopUpsell,            // 'desktop-upsell'
    LspRecommendation,        // 'lsp-recommendation'
    PluginHint,               // 'plugin-hint'
    // Panels (rendered inline — no overlay)
    TasksView, TeamsView, AgentsView, SettingsView, HelpView, AboutView, QuickOpen,
    // UI13 — agent wizard (create/edit).
    CreateAgent,        // 4-step new-agent wizard
    EditAgent,          // 4-step edit-agent wizard
    // UI8  — trust dialog (standalone takeover; 4-tier risk UX).
    TrustDialog,        // 'trust-dialog'
};

/// Input modes — unified canonical enum from cc::ui::common.
/// Previously this file defined a local 10-value InputMode:
///   {Prompt, Bash, SlashCommand, HistorySearch, PlanMode,
///    VimInsert, VimNormal, VimVisual, OrphanedPermission, TaskNotification}
/// Old→new mapping:
///   InputMode::Prompt → PromptInputMode::Normal  (TS: 'prompt')
/// All other values retain their names in the unified enum.
/// TS REF: src/types/textInputTypes.ts:265 (PromptInputMode type)
using InputMode = cc::ui::common::PromptInputMode;

// =========================================================
// Data structures (lean projections — engine owns full state)
// =========================================================

/// Status bar projection.  Mirrors TS REPL top status line.
struct StatusBarData {
    std::string model_name;
    std::optional<std::string> session_name, agent_name;
    std::optional<double> cost_usd;
    int input_tokens = 0, output_tokens = 0, context_token_count = 0;
    bool is_fast_mode = false, is_auto_mode = false, is_brief_mode = false;
    std::optional<std::string> effort_level;
    int swarm_session_count = 0;
    bool bridge_connected = false;
    std::optional<std::string> current_path;
};

/// Minimal Message projection for orchestration.
/// RFC 0002 F3: MessageDisplayEntry moved to cc.ui.screens.messages_store
/// (visible here through the re-export shim above; the shim is deleted in
/// F3 Finalize).

/// RFC 0002 F3: PermissionToolKind + PermissionRequestInfo (the permission-
/// prompt subset, TS ToolUseConfirm) moved to cc.ui.screens.permission_store
/// (visible here through the re-export shim above; the shim is deleted in
/// F3 Finalize). The dead nested DialogContext bridge struct (zero type
/// usages anywhere; repl_screen_dialog_queue.cpp notes there are ZERO writes
/// to it) was deleted rather than moved — project convention prefers
/// deleting dead code to relocating it.

///
/// Lean orchestration state.  Full app state lives in services/; the
/// engine writes computed projections into this struct between frames.
struct ReplScreenState {
    ReplMode mode = ReplMode::Normal;
    // RFC 0002 F3: prompt-input state (input mode, stashed prompt,
    // placeholder cascade inputs, teammate prefix color) sharded into
    // cc.ui.screens.prompt_store (PromptStore). StashedPrompt stays
    // visible here through the re-export shim at the top of this file;
    // the shim is deleted in F3 Finalize.
    PromptStore prompt_store;
    // RFC 0002 F3: task-view state (spinner mode, task notifications,
    // agent/teammate live state) sharded into cc.ui.screens.task_view_store
    // (TaskViewStore). SpinnerMode stays visible here through the re-export
    // shim at the top of this file; the shim is deleted in F3 Finalize.
    TaskViewStore task_view_store;
    // RFC 0002 F3: message-list / scroll / transcript-chrome state sharded
    // into cc.ui.screens.messages_store (MessagesStore). MessageDisplayEntry
    // stays visible here through the re-export shim at the top of this file;
    // the shim is deleted in F3 Finalize.
    MessagesStore messages_store;
    bool active_local_jsx_command = false;
    std::string active_local_jsx_command_name;
    std::string active_local_jsx_command_args;
    std::string active_local_jsx_content;
    int active_agents_selection_position = 0;
    // TS REF: Messages.tsx expandedKeys (L563) — Set of expand keys the user
    // has toggled open via Enter/Space on clickable rows.  Tool results and
    // thinking blocks render with verbose=true when their expand key is in
    // this set, showing full content instead of truncated summary.
    std::unordered_set<std::string> expanded_keys;

    /// TS REF: Messages.tsx isBriefOnly prop (L236, L510-514).
    /// When true, only brief-tool calls + their results + real user input
    /// are shown; assistant text, thinking, and non-brief tools are hidden.
    /// Toggled by the user (e.g., via /brief command or status bar click).
    bool is_brief_mode = false;

    /// TS REF: Messages.tsx L382-389 + L395-419  isStreamingThinkingVisible.
    /// When true, ALL completed thinking blocks are hidden (TS:
    /// lastThinkingBlockId = 'streaming').  Set by app.cppm when any streaming
    /// thinking entry is active or within its 30s grace period.
    bool streaming_thinking_globally_visible = false;

    /// TS REF: Messages.tsx isTranscriptMode (L459, screen === 'transcript').
    /// When true, the message list shows the FULL transcript (all message
    /// types visible, bypassing brief/dropText filters).  Capped at last 30
    /// messages unless show_all_in_transcript is also true.
    /// Toggled by Ctrl+O (TS: app:toggleTranscript global shortcut).
    bool is_transcript_mode = false;

    /// TS REF: Messages.tsx showAllInTranscript prop (L223, L467, L515-516).
    /// When true AND is_transcript_mode is true, the 30-message cap is lifted
    /// and ALL messages are rendered.  Toggled by Ctrl+E while in transcript
    /// mode (TS: transcript:toggleShowAll shortcut, Transcript context).
    bool show_all_in_transcript = false;

    // Input
    std::string input_text;
    // RFC 0002 F3: stashed prompt (StashedPrompt), input placeholder,
    // placeholder-cascade state (viewing_agent_name, submit_count,
    // queued_command_hint_shown_count, prompt_suggestion_enabled,
    // has_editable_queued_commands) and teammate_prefix_color moved to
    // PromptStore (prompt_store field above).
    // Welcome-header data (shown when messages is empty on a fresh session).
    std::string app_version = "0.0.0";
    std::string model_display_name;
    // TS LogoV2/CondensedLogo row 2 separator billing_type token (e.g.
    // "API Usage Billing" / "Team Seat" / "Rate Limited").  Empty = row 2
    // shows only the model name without " · <billing>" suffix.
    std::string billing_type;
    std::string cwd;
    // P0-6 builtin statusline: detected git branch for cwd (empty = not a git
    // repo or detection failed).  Populated by AppAdapter from
    // cc::utils::git::get_branch(), cached per-cwd-change to avoid spawning
    // `git` on every render tick.
    std::string git_branch;
    // M2: oauthAccount.displayName analogue — drives formatWelcomeMessage
    // ("Welcome back {user}!" vs "Welcome back!").  Empty for new users.
    std::string user_display_name;
    // P0 Gap logov2-render-modes-missing: pre-rendered feed content for the
    // full-logo horizontal mode's right column. Populated by the engine layer
    // from session storage (recent activity) and changelog parser (what's
    // new). Empty vectors fall back to placeholders defined in RenderWelcomeHeader.
    std::vector<std::string> recent_activity_lines;
    std::vector<std::string> changelog_lines;
    // TS REF: LogoV2.tsx L56 shouldShowProjectOnboarding() — when true, the
    // horizontal-layout feed column shows [ProjectOnboarding, RecentActivity]
    // instead of [RecentActivity, What'sNew].  Also prevents condensed mode
    // (TS isCondensedMode = !hasReleaseNotes && !showOnboarding && !forceFull).
    bool show_onboarding = false;
    // TS REF: LogoV2.tsx L70 useShowGuestPassesUpsell() — when true and no
    // onboarding, feed shows [RecentActivity, GuestPasses] instead of default.
    bool show_guest_passes_upsell = false;
    // TS REF: LogoV2.tsx L71 useShowOverageCreditUpsell() — when true and no
    // onboarding/guest-passes, feed shows [RecentActivity, OverageCredit].
    bool show_overage_credit_upsell = false;

    // ── P1 Footer notifications ──────────────────────────────────────
    // TS REF: src/components/PromptInput/Notifications.tsx
    // These fields drive the right-column notification area.
    // RenderNotifications() picks the highest-priority active item.
    cc::ui::prompt::footer::ApiKeyStatus api_key_status =
        cc::ui::prompt::footer::ApiKeyStatus::Unknown;
    bool is_remote_session = false;   // LOOM_REMOTE → changes error text
    bool debug_mode = false;          // "Debug mode" pill
    bool verbose = false;             // show token count when valid + verbose
    // IDE selection indicator (TS IdeStatusIndicator.tsx)
    bool ide_connected = false;
    std::optional<std::string> ide_file_path;
    std::optional<int> ide_selected_lines;
    // Dynamic notification (env-hook, external-editor hint, etc.)
    std::optional<std::string> footer_dynamic_text;
    std::optional<std::string> footer_dynamic_color;  // "error" / "warning" / empty=dim
    // P1: Notification queue — rotating carousel of up to 12 status notices.
    // TS REF: src/context/notifications.tsx (useNotifications hook)
    // Items are added via hooks (env-hook, rate-limit warnings, etc.) and
    // rotate through the footer's notification slot on a timeout basis.
    cc::ui::prompt::footer::NotificationQueue footer_notification_queue;
    // Stable per-session welcome-tip index (seeded once from the session id in
    // app.cppm). The renderer mods this by kWelcomeTips.size(). Previously the
    // tip used spinner_frame, which cycled the tip on every mouse-move re-render.
    std::size_t welcome_tip_index{0};
    struct AutocompleteSuggestion {
        std::string display_text;
        std::string description;
        std::string insert_text;
        std::size_t replacement_start = std::string::npos;
        std::size_t replacement_end = std::string::npos;
        bool submit_on_return = false;
        /// Optional icon prefix (e.g. "📄" for files, "📁" for dirs).
        /// TS REF: src/components/PromptInput/PromptInputFooterSuggestions.tsx:24
        ///          (getIcon — + for files, ◇ for MCP, * for agents).
        std::string icon{};
        // INF-02: stable identity for selection preservation across refreshes
        // (TS getPreservedSelection-by-id, src/hooks/useTypeahead.tsx:52-74).
        // Defaults to display_text when a caller doesn't supply a richer id,
        // so same-display items from different sources can be distinguished.
        // `{}` in-class init so existing partial designated initializers (e.g.
        // in tests/test_ui.cpp) don't trip -Wmissing-designated-field-initializers.
        std::string id{};
        /// Optional color name for a colored dot prefix (e.g. "red", "blue").
        /// TS REF: src/hooks/unifiedSuggestions.ts:77-108 — agent defs include
        ///          a color field used to tint the avatar dot in the picker.
        std::string color_name{};
    };
    std::vector<AutocompleteSuggestion> autocomplete_suggestions;
    // INF-05: input text at which the user dismissed the popup with Esc.
    // RefreshAutocompleteSuggestions stays dismissed until the input changes
    // (TS dismissedForInputRef, src/hooks/useTypeahead.tsx:893-908).
    std::string dismissed_autocomplete_for_input;
    // INF-03: precomputed stable name-column width for slash suggestions (max
    // over ALL candidates) so the description column doesn't jitter while
    // filtering. 0 = derive dynamically per visible row (@, #, ...).
    // Mirrors TS maxColumnWidth (src/hooks/useTypeahead.tsx:380-386).
    int autocomplete_stable_name_width = 0;
    // SL-03: pending inline argument hint for the slash command currently being
    // typed (e.g. "set <key> <value>"). Populated by RefreshAutocompleteSuggestions
    // when input matches "/cmd "; rendered by TextInputImpl.
    std::string pending_argument_hint;
    // SL-05: pending ghost-text completion suffix for a mid-input "/prefix"
    // command token (e.g. input "text /com" → ghost "mit"). Rendered inline
    // by TextInputImpl inline_ghost_text.
    std::string pending_ghost_text;
    // SL-11: deterministic next-action suggestion produced by
    // PromptSuggestionService on QueryEnd (no LLM). When set and the prompt
    // input is empty, RefreshAutocompleteSuggestions surfaces it as the lone
    // AutocompleteSuggestion; any non-empty input clears it. Acts as the
    // shared hand-off between the engine-side QueryEndHook (writer) and the
    // renderer (reader). Cleared on accept / first keystroke.
    std::optional<std::string> next_action_suggestion;
    int autocomplete_index = -1;
    std::size_t input_cursor = std::string::npos;
    std::deque<std::string> input_history;
    std::size_t history_index = std::string::npos;
    // AT-09: inbound IDE at_mentioned notifications deliver a fully-formed
    // "@<relpath>#L<a>-<b>" token to insert at the prompt cursor. They arrive
    // on the MCP receive thread, so they are staged under mutex in the AppImpl
    // composition layer and drained on the render thread
    // (DrainPendingAtMentionInserts -> ApplyPendingAtMentionInserts).
    // Faithful to TS useIdeAtMentioned.ts -> inputState.insert at cursor.
    // RFC 0002 F3: the staging mutex + queue moved OUT to AppImpl; the
    // DRAINED queue lives in McpStatusStore (mcp_status_store field below).
    // McpStatusStore stays visible here through the re-export shim at the
    // top of this file; the shim is deleted in F3 Finalize.
    McpStatusStore mcp_status_store;
    // Status / permission / context
    StatusBarData status_bar;
    // RFC 0002 F3: permission-prompt state (PermissionToolKind + the
    // ToolUseConfirm subset PermissionRequestInfo) sharded into
    // cc.ui.screens.permission_store (PermissionStore). PermissionRequestInfo
    // stays visible here through the re-export shim at the top of this file;
    // the shim is deleted in F3 Finalize.
    PermissionStore permission_store;
    // Settings-driven UI configuration (mirrors AppState.settings subset
    // that the renderer needs — populated by the engine/app layer).
    std::string settings_model;             // Configured default model
    std::string settings_agent_name;        // Configured settings.agent (TS getInitialSettings().agent)

    // Bridge / remote-control footer projection (TS replBridge* AppState
    // fields; projected from AppStore in SyncState).
    bool bridge_enabled = false;
    bool bridge_explicit_remote = false;
    bool bridge_connected = false;
    bool bridge_session_active = false;
    bool bridge_reconnecting = false;
    bool bridge_selected = false;           // footer item focused (IDE selection)

    // Footer pasting indicator (TS usePasteHandler.ts isPasting +
    // PASTE_COMPLETION_TIMEOUT_MS = 100ms). A multi-char event (terminal
    // paste arrives as one batch) stamps this point; RenderLeftSide shows
    // "Pasting text…" for 100ms after it. Event-driven — no ticker.
    std::optional<std::chrono::steady_clock::time_point> pasting_since;

    // TS REF: src/hooks/useTextInput.ts:126-153 handleEscape via
    // src/hooks/useDoublePress.ts:6 DOUBLE_PRESS_TIMEOUT_MS = 800.
    // First Esc on non-empty input arms (notification "Esc again to clear");
    // a second Esc within 800ms persists to prompt history and clears.
    // Event-driven — no timer (same pattern as pasting_since).
    std::optional<std::chrono::steady_clock::time_point> escape_pending_since;

    // TS REF: src/hooks/useTextInput.ts:108-120 handleCtrlC double-press —
    // useExitOnCtrlCD exitState projected for
    // LeftSideOptions.exit_message_show. Presence of the timestamp = show;
    // expiry is event-driven at render (pasting_since pattern). The app
    // layer owns the exit decision; the screen only renders the window.
    std::optional<std::chrono::steady_clock::time_point> exit_message_until;
    std::string exit_message_key = "Ctrl-C";

    bool status_line_enabled = false;       // User-configurable status line
    std::string status_line_command;        // Shell command for status line
    int status_line_padding = 0;            // Horizontal padding for status line
    std::string status_line_text;           // Cached output of the status line command (may contain ANSI)
    // Counts and live-teams state moved to TaskViewStore (task_view_store
    // field above) in RFC 0002 F3.

    // Permission mode (cycled via shift+tab; TS REF: getNextPermissionMode.ts)
    cc::ui::prompt::footer::PermissionMode permission_mode =
        cc::ui::prompt::footer::PermissionMode::Default;
    // Dialog-suppression flag (typing -> suppress interrupt dialogs)
    bool is_prompt_input_active = false;
    // M7: True when a JSX tool result is currently rendering an animation in
    // the message stream.  When set, Band3 dialogs (ToolPermission overlay,
    // PromptDialog, Elicitation) are suppressed to avoid visual clash.
    bool is_tool_animation_active = false;
    std::chrono::steady_clock::time_point last_keystroke;

    // RFC 0002 F3: dialog state (overlay dialogs, inline panels, wizard /
    // trust component handles, the M7 dialog queue + renderer registry)
    // sharded into cc.ui.screens.dialog_store (DialogStore). The component
    // handles and DialogQueue stay visible here through the re-export shim
    // at the top of this file; the shim is deleted in F3 Finalize.
    DialogStore dialog_store;
};

/// Engine-facing callbacks (TS ReplScreen external prop callbacks).
struct ReplScreenCallbacks {
    std::function<void(const std::string&, InputMode)> on_submit;
    std::function<void()> on_interrupt;                 // Ctrl+C
    std::function<void()> on_exit;                      // Ctrl+D or /exit
    /// TS REF: src/keybindings/defaultBindings.ts:42 'ctrl+l':'app:redraw'
    /// (Global context) + useGlobalKeybindings.tsx handleRedraw ->
    /// ink forceRedraw (ERASE_SCREEN '\x1b[2J' + CURSOR_HOME '\x1b[H', then
    /// repaint). The engine forces a terminal repaint WITHOUT mutating the
    /// input text or autocomplete state.
    std::function<void()> on_redraw;
    /// TS REF: src/hooks/useTextInput.ts:142-150 — Esc double-press clear
    /// persists the original value via addToHistory before clearing. The
    /// engine owns the session id / project cwd for the history append.
    std::function<void(const std::string&)> on_save_to_history;
    // Legacy simple permission response (allow/deny + always flag)
    std::function<void(bool, std::optional<bool>)> on_permission_response;
    // M6: Rich permission response — decision kind, scope, and feedback text.
    // Mirrors TS onPermissionRequestDecision callback.
    std::function<void(
        std::string_view decision,  // "allow_once", "allow_always", "deny", "abort"
        std::string_view scope,     // "session", "global", "project", "loom_folder", etc.
        std::string_view feedback   // user feedback text, may be empty
    )> on_permission_decision;
    std::function<void(ReplMode, int)> on_dialog_action;
    std::function<void(ReplMode)> on_mode_change;
    std::function<void(const std::string& command)> enqueue_slash_command;
    std::function<std::optional<cc::ui::agents::cards::AgentCardData>(
        std::string_view agent_id)> load_agent_for_wizard;
    std::function<void(const cc::ui::agents::wizard::WizardDraft& draft)> save_agent_from_wizard;
    std::function<void()> on_local_jsx_cancel;
    std::function<bool(Event)> on_local_jsx_event;
    /// Called when user cycles permission mode (shift+tab).  TS REF:
    /// PromptInput.tsx:1409 handleCycleMode → cyclePermissionMode().
    std::function<void(cc::ui::prompt::footer::PermissionMode)> on_permission_cycle;
    /// GAP 3: msg-system-api-error-retry — called when user clicks "Retry"
    /// on an API error message.  Re-sends the last user message.
    /// TS REF: src/components/messages/SystemAPIErrorMessage.tsx — the
    ///   retry button re-triggers the last user submission.
    std::function<void()> on_retry;
    /// P2 gap api-error-retry: called when user clicks "Clear session"
    /// on a session-expired API error card.  Resets the conversation so
    /// the user can re-authenticate.
    /// TS REF: SystemAPIErrorMessage.tsx — onClearSession prop.
    std::function<void()> on_clear_session;
    /// TS REF: Messages.tsx L703-712 + Markdown.tsx L186-235 — shared
    /// StreamingMarkdown instance for the streaming-text tail row.
    /// When non-null, RenderMessages threads it to the messages list
    /// so is_streaming rows use stable-prefix caching.
    ::cc::ui::StreamingMarkdown* streaming_md = nullptr;
};

}  // namespace cc::ui
