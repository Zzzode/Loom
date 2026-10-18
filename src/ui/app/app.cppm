/// @file app.cppm
/// @brief Application entry point — thin adapter that drives repl_screen from
///        the production QueryEngine.
module;


#include <unistd.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <cctype>

export module loom.ui.app.app;

import std;

import loom.types.types;
import loom.ui.visual.markdown;  // arch-check: keep-import (StreamingMarkdown field)
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;   // MessageDisplayEntry (project_message / local_command_messages_)
import loom.ui.screens.task_view_store;  // PendingSpinner mode
import loom.ui.prompt.autocomplete_sources;
// P0-2: 7-stage message pipeline utilities (dedup / tag filter / tool augment).
import loom.ui.messages.message_pipeline;
// P3-1c: async statusline worker (jthread + debounce/memo state).
import loom.ui.app.statusline_coordinator;  // arch-check: keep-import (member)
// P3-1a: teammate inbox/permission state (PIMPL + self-contained methods).
import loom.ui.app.teammate_coordinator;  // arch-check: keep-import (member)
// P4-1b: async clipboard paste state + worker.
import loom.ui.app.paste_coordinator;  // arch-check: keep-import (member)
// P4-1d: tool-permission + elicitation + ask-user blocking-response state.
import loom.ui.app.permission_coordinator;  // arch-check: keep-import (member)
// RFC 0007 Phase 1: async clipboard copy coordinator.
import loom.platform.clipboard_coordinator;  // arch-check: keep-import (member)

export namespace loom::ui {

// PIMPL backing type, defined in the internal partition loom.ui.app.app:impl.
// Forward-declared here so the interface can hold a unique_ptr without
// importing the heavy modules its member objects require.
struct AppImpl;
// Deleter whose call operator is defined in the :impl partition (where
// AppImpl is complete). This lets unique_ptr<AppImpl> be destroyed from any
// translation unit — including the out-of-line constructor's implicit
// cleanup — without AppImpl being complete there.
struct AppImplDeleter {
    void operator()(AppImpl* p) const noexcept;
};

// Nested PIMPL for settings (disk load + file-watch). Defined in
// app_settings.cpp; keeps loom.config.settings_manager out of the :impl BMI.
struct SettingsState;
struct SettingsStateDeleter {
    void operator()(SettingsState* p) const noexcept;
};

// Defined in app_store_bridge.cpp; type-erased shared_ptr factory so :impl
// never imports loom.state.{store,app_state}.
[[nodiscard]] std::shared_ptr<void> create_typed_app_store();

// Plain-data projection of the AppState bridge fields, returned by
// AppAdapter::bridge_state() so callers need not import loom.state.app_state.
struct BridgeState {
    bool enabled = false;
    bool explicit_remote = false;
    bool connected = false;
    bool session_active = false;
    bool reconnecting = false;
};

// SL-11: defined in app_prompt_suggestion_wiring.cpp (impl unit) to keep the
// heavy loom.services.prompt_suggestion import out of this thin module (clang
// 2GB source-location budget).
void wire_prompt_suggestion_hook(void* hooks, void* engine,
                                 std::function<void(std::optional<std::string>)> publish);

using namespace ftxui;
using namespace loom::core;

namespace repl = loom::ui::repl_screen;
namespace acsrc = loom::ui::autocomplete_sources;

// Env/config + text/UTF free helpers — bodies in app_helpers.cpp
// (RFC 0001 Phase C batch 1). Plain declarations here (inline dropped;
// the impl unit holds the single strong definition).
[[nodiscard]] std::optional<std::string> non_empty_env(const char* name);
[[nodiscard]] std::optional<std::string> first_non_empty_env(
    std::initializer_list<const char*> names);
[[nodiscard]] std::optional<bool> parse_bool_text(const std::string& value);
[[nodiscard]] std::optional<int> parse_int_text(const std::string& value);
[[nodiscard]] std::string trim_ascii_copy(std::string_view value);
[[nodiscard]] std::string summarize_agent_description(
    std::string_view description);
[[nodiscard]] std::string lowercase_ascii(std::string_view value);

// Session-transcript helpers shared between the session picker
// (app_handle_submit.cpp) and autocomplete (app_autocomplete.cpp).  Both read
// messages.jsonl only — the legacy messages.json format is no longer
// supported.  Bodies in app_handle_submit.cpp.
[[nodiscard]] int count_session_messages(
    const std::filesystem::path& sessions_dir,
    const std::string& session_id);
[[nodiscard]] std::string first_user_message_as_title(
    const std::filesystem::path& sessions_dir,
    const std::string& session_id);


struct AutocompleteToken {
    std::size_t start = 0;
    std::size_t end = 0;
    std::string text;
};

// Body in app_helpers.cpp (RFC 0001 Phase C batch 1).
[[nodiscard]] bool ascii_isspace(char ch);

// Body in app_autocomplete.cpp (RFC 0001 Phase C batch 2) — sole caller
// is RefreshAutocompleteSuggestions in the same TU. inline dropped; the
// impl unit holds the single strong definition.
[[nodiscard]] AutocompleteToken token_around_cursor(
    std::string_view input,
    std::size_t cursor);

// AT-12: fuzzy_match_ascii / fuzzy_rank_ascii removed — all autocomplete
// ranking now delegates to loom::ui::prompt::fuzzy_rank_nucleo (frn::), which
// ports the nucleo/fzf-v2 scorer (boundary/camel/consecutive/gap/path bonuses)
// while preserving the exact {0..3} base range so the tier offsets (alias +1,
// skill +4, plugin +6) and the rank-ascending sort stay unchanged. See
// ui/prompt/fuzzy_rank_nucleo.cppm. lowercase_ascii() above is retained.

// ============================================================
// Projection: Engine state -> ReplScreenState
// ============================================================

[[nodiscard]] repl::MessageDisplayEntry project_message(const Message& msg);

// ============================================================
// project_messages — projection that splits a single
// AssistantMessage into MULTIPLE display rows when it mixes a ThinkingBlock
// with a TextBlock / ToolUseBlock.  These render as separate sibling
// messages (a collapsed `∴ Thinking` row followed by the visible answer /
// tool-use row); the legacy single-entry projection collapsed them into one
// thinking row, which hid the visible answer once M4 routed thinking rows
// through RenderThinkingMessageFaithful (collapsed → raw text hidden).
//
// Non-assistant messages and assistant messages with a single block kind
// still project to exactly one entry (identical to project_message).
// ============================================================
[[nodiscard]] std::vector<repl::MessageDisplayEntry>
project_messages(const Message& msg);

void reconcile_tool_result_statuses(std::vector<repl::MessageDisplayEntry>& rows);

// ============================================================
// Convenience: render a single core Message to an Element.
// Used by tests and callers that want a quick rendering of one message.
// ============================================================

[[nodiscard]] Element RenderMessage(const Message& msg);

// ============================================================
// App Adapter Component
// ============================================================

class AppAdapter : public ComponentBase {
    // P5: testing seams are private; AppTestingSeams (in
    // loom.ui.app.testing_seams) is the only caller.
    friend struct AppTestingSeams;

private:
    // UI-thread configured; snapshotted when launching a query worker.
    std::optional<std::uint32_t> tool_round_budget_;
    std::unique_ptr<AppImpl, AppImplDeleter> impl_;
    // P3-1a: teammate inbox/permission cluster.  Owns the PIMPL that
    // used to live here as unique_ptr<TeammateState, TeammateStateDeleter>.
    TeammateCoordinator teammate_;
    std::unique_ptr<SettingsState, SettingsStateDeleter> settings_;
    // Defined in the :impl partition where AppImpl is complete. The out-of-line
    // constructor body calls this; teardown goes through AppImplDeleter, so
    // neither impl unit needs AppImpl's layout.
    // Stored type-erased as void* in AppImpl; each impl unit casts back
    // after importing the owning module (loom.query/loom.hooks/loom.commands),
    // keeping those closures out of this BMI.  The sessions directory is
    // NOT type-erased: it is a standard fs::path resolved once in
    // construct_impl and exposed via sessions_dir().
    void construct_impl(void* engine, void* lifecycle_hooks,
                        void* cmd_registry,
                        std::optional<std::filesystem::path> sessions_dir);
    void construct_settings();
    [[nodiscard]] void* engine_raw() const noexcept;
    [[nodiscard]] void* lifecycle_hooks_raw() const noexcept;
    [[nodiscard]] void* cmd_registry_raw() const noexcept;
    [[nodiscard]] std::filesystem::path sessions_dir() const noexcept;

    std::function<void()> on_exit_;

    // OSC 7501 lifecycle stays behind AppImpl; painting never sends reports.
    void StartProgramStatusWithWriter(bool interactive,
        std::function<bool(std::string_view)> writer);
    [[nodiscard]] std::uint64_t BeginProgramStatusOperation();
    void CancelProgramStatusOperation();
    void CompleteProgramStatusOperation(std::uint64_t generation, bool error, bool paused);
    [[nodiscard]] bool HandleProgramStatusControl(const Event& event);
    void PrepareProgramStatusInput(Event& event);
    [[nodiscard]] std::pair<bool, bool> ProgramStatusBlocks(); // permission, question
    void UpdateProgramStatusSnapshot();
    void RefreshProgramStatus() noexcept;

    std::shared_ptr<repl::ReplScreenState> screen_state_;
    Component repl_component_;
    std::vector<repl::MessageDisplayEntry> local_command_messages_;
    /// Retained copy of the on_clear_session callback so tests can trigger
    /// the session-expired card's "Clear session" button path directly.
    std::function<void()> clear_session_callback_;

    std::string current_session_id_;
    // The observer owns only this shared dirty flag and the UiActions handoff;
    // neither capture borrows AppAdapter's lifetime.
    std::shared_ptr<std::atomic<bool>> runtime_agents_dirty_ =
        std::make_shared<std::atomic<bool>>(true);
    std::size_t runtime_agents_subscription_ = 0;
    std::string projected_runtime_agent_session_;
    std::uint64_t runtime_agent_generation_ = 0;
    std::string agent_instances_dialog_id_;
    std::string agent_instances_session_;
    std::shared_ptr<void> agent_instances_state_;
    std::jthread agent_operation_thread_;
    struct AgentConversationView {
        std::shared_ptr<repl::ReplScreenState> state;
        Component component;
        std::uint64_t generation = std::numeric_limits<std::uint64_t>::max();
        std::string root_session;
        std::string id;
        std::string title;
    };
    std::unordered_map<std::string, AgentConversationView> agent_conversation_views_;
    std::string active_agent_conversation_;
    void OpenAgentConversation(std::string session, std::string id);
    void RefreshAgentConversation();
    [[nodiscard]] bool SubmitAgentConversation(std::string session, std::string id,
        const std::string& text, repl::InputMode mode);
    [[nodiscard]] bool RootDialogTakesPriority();
    [[nodiscard]] Component VisibleConversationComponent();
    [[nodiscard]] bool HandleAgentConversationEvent(const Event& event);

    // Ctrl-C double-press ExitHandler moved into AppImpl (:impl partition);
    // access via set_exit_message_impl/reset_exit_handler/handle_ctrl_c.

    // Session start time for duration tracking (statusline cost.total_duration_ms)
    std::chrono::steady_clock::time_point session_start_time_;

    // Async query state
    std::jthread query_thread_;
    std::jthread spinner_thread_;
    // Local '!' bash command worker. Runs the command outside the LLM turn
    // (shouldQuery:false), so it uses its own thread rather than query_thread_
    // and never sets query_running_.
    std::jthread bash_thread_;
    std::atomic<bool> bash_running_{false};
    std::atomic<bool> query_running_{false};

    // ── Teammate inbox worker state moved into AppImpl (:impl/:team). ─────

    // ── Live-teammate projection (leader UI) ───────────────────────────────
    // Event-driven pane observer: background callbacks only flag + post an
    // FTXUI event; projection merges snapshots on the UI thread.
    std::uint64_t pane_observer_token_ = 0;
    // Held for the subscription's lifetime; releasing it on destruction makes
    // an in-flight observer callback a no-op (no use-after-free on `this`).
    std::shared_ptr<void> pane_observer_sub_guard_;
    std::atomic<bool> pane_snapshot_dirty_{false};
    // Last serialized projection; suppresses no-op state replacement.
    std::string projected_teams_signature_;

    // ── Leader-side teammate permission requests ──────────────────────────
    // Pending-permission deque + mutex moved into AppImpl (:impl/:team).

    // ── Leader inbox poller (team-lead mailbox) ────────────────────────────
    // Mirrors the pane-teammate inbox worker: a background jthread polls
    // $ROOT/<team>/inboxes/team-lead.json for stage-A permission_request
    // envelopes, enqueues them (mutex + PostRenderEvent), and the UI thread
    // drains them into the existing ToolPermission dialog. Timed mailbox
    // polling is allowed; there is still no FTXUI render ticker.
    std::jthread leader_inbox_thread_;
    std::unordered_set<std::string> seen_leader_permission_ids_;
    /// P2 gap api-error-retry: last user-submitted message text.  Used by
    /// the Retry button on SystemAPIError cards to re-send the same query.
    /// The Retry handler re-submits this text.
    std::string last_submitted_text_;
    // Cached autocomplete data (loaded once at startup to avoid repeated disk I/O
    // on every keystroke — these are memoized).
    std::vector<acsrc::SkillSuggestionData> cached_skills_;
    std::vector<acsrc::PluginCommandSuggestionData> cached_plugin_commands_;
    std::atomic<std::uint64_t> ui_animation_tick_count_{0};
    std::atomic<bool> welcome_animation_active_{true};
    std::atomic<std::int64_t> thinking_collapse_deadline_ms_{0};
    std::atomic<std::int64_t> paste_submission_deadline_ms_{0};
    std::atomic<std::int64_t> chain_label_deadline_ms_{0};
    std::atomic<std::int64_t> generation_rate_deadline_ms_{0};
    std::atomic<std::int64_t> todo_collapse_deadline_ms_{0};
    std::atomic<bool> conversation_projection_dirty_{true};
    struct CachedMessageProjection {
        Message source;
        std::vector<repl::MessageDisplayEntry> rows;
    };
    std::vector<CachedMessageProjection> committed_projection_cache_;
    std::size_t committed_row_count_ = 0;
    std::uint64_t committed_source_count_ = 0;
    struct PendingPasteSubmission {
        std::string text;
        repl::InputMode mode;
        std::chrono::steady_clock::time_point deadline;
    };
    std::optional<PendingPasteSubmission> pending_paste_submission_;
    std::mutex result_mutex_;
    // RFC 0018: staged under result_mutex_; only the UI projects the rate.
    struct GenerationRate {
        struct Sample {
            std::chrono::steady_clock::time_point at;
            std::size_t tokens;
        };
        std::optional<std::uint32_t> block_index;
        std::optional<std::chrono::steady_clock::time_point> last_delta;
        std::deque<Sample> samples;
        std::optional<double> value;
        bool suppressed = false;
    } generation_rate_;
    // Caller holds result_mutex_. Reset retains cancellation suppression.
    void ResetGenerationRate(bool suppress = false);
    void ObserveGenerationDelta(std::uint32_t index,
                                std::chrono::steady_clock::time_point now);
    void ProjectGenerationRate();
    /// Pending error from a StreamError event.  Carries the error_type so
    /// ConsumePendingResult can distinguish API/network errors (retriable,
    /// show countdown + Retry button) from tool errors (not retriable,
    /// show Dismiss only).
    struct PendingError {
        std::string type;
        std::string message;
    };
    std::optional<PendingError> pending_error_;
    // Worker-owned stream transitions; applied to the plain store on the UI thread.
    struct PendingSpinner {
        repl::SpinnerMode mode;
        std::optional<std::string> verb;
    };
    std::optional<PendingSpinner> pending_spinner_;
    bool pending_markdown_reset_ = false;
    std::atomic<bool> statusline_update_pending_{false};
    // P4-1b: async clipboard paste state + worker extracted to
    // PasteCoordinator.  AppAdapter keeps ProcessCompletedPastes and
    // DrainCompletedPastesAndSubmit (they need screen_state_) and accesses the
    // coordinator's state through its accessors.
    PasteCoordinator paste_;
    // Local '!' bash command output posted back from bash_thread_ (bg→UI),
    // drained on the render thread in ConsumePendingResult().  Mirrors the
    // pending_paste_results_ handoff pattern so local_command_messages_ /
    // SyncState are only ever mutated on the render thread.
    std::mutex bash_result_mutex_;
    struct PendingBashResult {
        std::string output;   // combined stdout+stderr (already trimmed)
        bool is_error = false;
        std::string cwd;
        std::string session_id;
    };
    std::optional<PendingBashResult> pending_bash_result_;  // bg→UI
    // AT-09: inbound IDE at_mentioned tokens staged by the MCP receive
    // thread (bg→UI), drained on the render thread in
    // DrainPendingAtMentionInserts into screen_state_->mcp_status_store.
    // Moved here from ReplScreenState in RFC 0002 F3: stores are
    // UI-thread-affined plain data and hold no mutex, so the staging queue
    // and its lock live in the composition layer; the store holds only the
    // drained data.
    std::mutex at_mention_mutex_;
    std::vector<std::string> pending_at_mention_inserts_;  // bg→UI
    std::string streaming_text_;
    /// Block index of the streaming text block.  The text block's index is
    /// not implied by its position — the model can emit text before tool_use
    /// (thinking → text → tool_use).  Tracking the actual index ensures the
    /// streaming projection places text in the correct order.
    std::optional<std::uint32_t> streaming_text_index_;
    /// StreamEnd stops live-generation styling; it does not remove the body.
    bool streaming_finalizing_ = false;
    /// Set only after this API round's AssistantMessage has been committed.
    /// Protected by result_mutex_, along with the streaming buffers.
    bool streaming_message_committed_ = false;
    /// StreamingMarkdown frame cache for the streaming-text tail
    /// row.  Reset alongside streaming_text_ so each new model response
    /// starts with a fresh frame.  Used by
    /// RenderAssistantTextMessageFaithful via MessagesListInput.streaming_md.
    ::loom::ui::StreamingMarkdown streaming_markdown_;
    struct StreamingToolPreview {
        std::string tool_name;
        std::string tool_use_id;  ///< M6: matches ToolExecution* events
        std::string input_json;
        std::string result_preview;  ///< M6: live streaming result preview
        // P0-2 Stage 5: Augmented tool result fields (lazy computed once on
        // ToolExecutionEnd — used by collapsed tool card + transcript.
        std::string compact_preview;   /// 200-char one-liner
        int         error_code      = 0;  /// 0=none, >0 shell exit/HTTP code
        bool        truncated       = false;/// result > 4 KiB threshold
        bool complete = false;       ///< ContentBlockStop: input_json fully streamed
        bool exec_started = false;   ///< Execution state is independent of output text
        bool exec_done = false;      ///< ToolExecutionEnd: tool has finished executing
        bool is_error = false;
        /// Wall-clock start time (steady_clock) — set on ContentBlockStart.
        /// Used by the chain compressor to include tool call time in the
        /// live elapsed timer (now - earliest_start across thinking + tools).
        std::optional<std::chrono::steady_clock::time_point> streaming_started_at;
    };
    // StreamingThinking preview { text, complete, streaming_ended_at,
    // streaming_started_at }.  streaming_ended_at enables the 3s grace
    // period after thinking stops (matching kThinkingCollapseGrace in
    // messages_list_payload_row.cpp).  streaming_started_at enables
    // duration computation for the collapsed "∴ Thought for Xs" label.
    struct StreamingThinkingPreview {
        std::string text;
        bool complete = false;
        std::optional<std::chrono::steady_clock::time_point> streaming_ended_at;
        std::optional<std::chrono::steady_clock::time_point> streaming_started_at;
    };
    std::map<std::uint32_t, StreamingToolPreview> streaming_tools_;
    std::map<std::uint32_t, StreamingThinkingPreview> streaming_thinking_;

    // Cache of thinking timing, keyed by thinking text content.
    // Populated when a streaming thinking block completes (ContentBlockStop);
    // consumed when projecting committed thinking entries (which lack timing
    // info because ThinkingBlock in the core types has no duration field).
    // The start_time is cached so the chain compressor's live elapsed timer
    // (now - earliest_start) survives the streaming→committed transition —
    // without it, earliest_start shifts forward when the streaming entry is
    // pruned and the elapsed time jumps backwards.
    struct ThinkingTiming {
        std::chrono::milliseconds duration{0};
        std::chrono::steady_clock::time_point start_time{};
    };
    std::unordered_map<std::string, ThinkingTiming>
        thinking_duration_cache_;

    // Returns true when any streaming thinking block is still being streamed,
    // OR when a recently-completed thinking block is within the 3-second
    // grace period.
    // Keeps the in-flight thinking projection (and its streaming tail)
    // alive for 3s after ContentBlockStop fires, so the just-finished
    // thinking row stays expanded before collapsing to the summary label.
    // Body in app_autocomplete.cpp (RFC 0001 Phase C batch 2).
    bool is_streaming_thinking_visible() const;
    // P0-2 Stage 1: per-turn dedup tracker for ContentBlock index transitions.
    // One per App (one instantiation per repl lifetime; cleared on each turn start.
    loom::ui::messages::pipeline::DedupTracker event_dedup_;
    std::atomic<ScreenInteractive*> screen_{nullptr};

    // ── Text selection (drag-to-select) ──────────────────────────────────
    // Left-click drag in the transcript selects text; release auto-copies
    // through the clipboard coordinator. Three-state machine: Idle → Potential
    // (press recorded, drag not yet confirmed) → Dragging (moved by any cell).
    // Clicks without drag pass through to existing handlers (hyperlink,
    // click-to-expand) which all listen on Released events.
    struct TextSelection {
        bool active = false;     // true once drag threshold exceeded
        bool potential = false;  // true after press, before threshold
        bool copied = false;     // true only after native clipboard completion
        std::size_t copied_chars = 0;  // UTF-8 char count for the toast
        std::optional<std::string> copy_feedback; // Confirmed copy or unconfirmed terminal request.
        int start_x = 0, start_y = 0;
        int end_x = 0, end_y = 0;
        // Transcript endpoints are content rows; other selections use screen
        // rows. Retain only frames visited during this selection gesture.
        struct TranscriptRows {
            std::map<std::pair<int, int>, std::vector<std::string>> cells;
            std::map<int, int> heights;
            int width = 0;
            bool invalid = false;
        };
        std::shared_ptr<TranscriptRows> rows;
        std::pair<int, int> start_row{}, end_row{};
        bool virtual_mode = false;
        int content_height = 0;
        std::uint64_t source_revision = 0;
    };
    TextSelection text_selection_;
    // ScreenInteractive clears its pixels after flushing each frame. Keep the
    // last rendered frame for selection extraction between render passes.
    // Shared ownership lets a returned render node safely retain the snapshot.
    std::shared_ptr<ftxui::Screen> text_selection_frame_;

    // Generation counter for stale-result detection: advanced on each copy
    // request post; re-checked in the result callback to discard stale
    // notifications from superseded selections.
    std::atomic<std::uint64_t> copy_generation_{0};
    // Pending copy result from the worker thread, drained on the UI thread.
    // The worker stores the result + generation here and calls PostRenderEvent;
    // the UI thread drains it in DrainCopyResult() (called from OnEvent/Render).
    struct PendingCopyResult {
        std::uint64_t generation = 0;
        loom::utils::clipboard::CopyResult result;
        std::shared_ptr<const std::string> text;  // for OSC 52 fallback
    };
    std::mutex copy_result_mutex_;
    std::optional<PendingCopyResult> pending_copy_result_;

    // RFC 0007 Phase 1: async clipboard copy coordinator.
    // The UI thread posts copy requests; the coordinator's owner thread
    // serializes cancel/reap/start of clipboard backend jobs. The destructor
    // calls shutdown() (bounded ~1.1s worst case) — the only blocking wait
    // on the UI thread, and explicitly a shutdown-path exception.
    // Declared AFTER copy_result_mutex_/pending_copy_result_ so that the
    // coordinator's destructor (which joins the worker) runs before the
    // mutex/optional are destroyed (defense-in-depth, P2-3).
    loom::utils::clipboard::ClipboardCoordinator clipboard_coordinator_;

    // ── Mouse tracking runtime toggle (F2) ──────────────────────────────
    // mouse_mode_ is the config value (mouseMode: true → FTXUI TrackMouse
    // on).  mouse_tracking_on_ is the runtime toggle state — F2 switches
    // between LOOM receiving mouse events (scroll wheel, clicks, in-app
    // drag-select) and the terminal handling mouse natively (drag to
    // select, Cmd+C to copy).  Only active when mouse_mode_ is true.
    bool mouse_mode_ = true;
    bool mouse_tracking_on_ = true;
    void ToggleMouseTracking();

    // ── Drag-to-select text selection (body in app_text_selection.cpp) ──
    // Handle mouse drag events for text selection. Returns true if the
    // event was consumed (drag active or completed).
    bool HandleTextSelectionMouse(Event event);
    // Wrap an Element with the selection-highlight node when a drag is
    // active.  Returns the element unchanged otherwise.
    Element ApplySelectionHighlight(Element el);
    // RFC 0007 Phase 1: drain pending clipboard copy results from the
    // worker thread onto the UI thread. Called from OnEvent/Render.
    void DrainCopyResult();
    // Clear all text selection state (active, potential, copied, chars).
    void ClearTextSelection();

    // P4-1d: tool-permission + MCP-elicitation + ask-user blocking-response
    // state extracted to PermissionCoordinator.  AppAdapter keeps the dialog
    // display logic and calls the coordinator's reset/wait/resolve methods.
    std::shared_ptr<PermissionCoordinator> permission_ = std::make_shared<PermissionCoordinator>();

    // Vim mode — state lives in AppImpl (:impl partition). Accessors keep the
    // VimMode/VimStateMachine types out of this interface.
    bool vim_enabled() const noexcept;
    void set_vim_enabled(bool on);
    // Returns the statusline mode label ("NORMAL"/"INSERT"/…) or nullopt when
    // vim mode is off.
    std::optional<std::string> vim_statusline_label() const;

    // Exit handler (Ctrl-C double-press) — state in AppImpl. Accessors keep
    // ExitHandler/ExitReason types out of this interface.
    void set_exit_message_impl(std::string_view msg);
    void reset_exit_handler();
    bool handle_ctrl_c();

    // AppStore bridge — state in AppImpl.
    bool has_app_store() const noexcept;
    [[nodiscard]] void* app_store_raw() const noexcept;
    BridgeState bridge_state() const;

    // Settings manager — state in AppImpl.
    void init_settings_manager();
    void subscribe_settings_changed(std::function<void()> cb);
    std::optional<std::string> setting_string(std::string_view key) const;
    std::optional<std::string> statusline_setting(std::string_view key) const;
    std::string output_style_setting() const;

    // Settings manager moved into AppImpl (:impl partition).
    std::function<void()> skills_changed_unsubscribe_;  // SkillRegistry dynamic discovery

    // Cost threshold hook — listener ID + shown guard to avoid re-prompting.
    int cost_listener_id_ = -1;
    bool cost_threshold_shown_ = false;

    // Redux-like AppState store moved into AppImpl (:impl partition).
    // Access via has_app_store/app_store_raw/bridge_state.

    // P3-1c: async statusline worker (jthread + debounce/memo state).
    // Owns the thread, atomics, mutex, cv, and memo cache that used to live
    // here as 9 separate fields.  Callbacks are injected in construct().
    StatuslineCoordinator statusline_;

    // P0-6 builtin statusline: git branch detection cache.  We only re-run
    // `git rev-parse --abbrev-ref HEAD` when the cwd changes (cd events are
    // rare).  This avoids spawning a subprocess on every render tick.
    std::string last_branch_cwd_;
    std::string cached_git_branch_;

    // Advance the bounded welcome intro and post one-shot expiry events.
    // Query spinner frames advance through stream/commit-driven renders.
    void StartUiAnimationTicker();
    // Stop/unblock/join workers while all of their dependencies are alive.
    void StopBackgroundWorkers();
    // App-local handoff for callbacks that may outlive their originating worker.
    using UiDispatcher = std::function<void(std::function<void()>)>;
    [[nodiscard]] UiDispatcher ui_dispatcher();
    void DrainUiActions();
    void CloseUiActions();
    void StopSettingsWatcher();

    // Post(Event::Custom) helper — body in app_animation.cpp (RFC 0001
    // Phase C batch 2). Private member; callable from member functions in
    // any impl unit (private access holds within member functions).
    void PostRenderEvent();

    // Stream-event handler — body in app_handle_submit.cpp. The query
    // thread's on_event callback and the test seam both delegate here.
    void handle_stream_event(const core::StreamEvent& ev);
    void handle_committed_message(const Message& message);

    // AT-09: drain the MCP at_mentioned staging queue (at_mention_mutex_ /
    // pending_at_mention_inserts_) into screen_state_->mcp_status_store on
    // the render thread, then apply it to the prompt. Body in
    // app_autocomplete.cpp.
    std::size_t DrainPendingAtMentionInserts();

    // ── Teammate inbox worker ───────────────────────────────────────────────

    // Teammate inbox worker methods used by other shards are declared here;
    // their bodies (and the partition-only helpers) live in the :team
    // partition (app_team.cppm), keeping the TeammateMessage/swarm closure out
    // of this interface BMI.
    [[nodiscard]] bool running_as_pane_teammate() const;
    bool drain_one_teammate_prompt();
    void start_teammate_inbox_worker();
    // Leader-side poller; body in the :team partition.
    void start_leader_inbox_worker();
    void enqueue_teammate_prompt(std::string prompt);
    void poll_teammate_inbox_once(const std::string& agent, const std::string& team);

    // Local-command / local-JSX rows — bodies in app_local_command.cpp
    // (RFC 0001 Phase C batch 2). AppendLocalCommandMessage's default
    // argument stays on this declaration; the out-of-line definition
    // omits it (a default must not be redefined by a later declaration
    // in the same scope).
    void AppendLocalMessagesToScreenState();

    void AppendLocalCommandInputMessage(std::string command);

    void AppendLocalCommandMessage(std::string message, bool is_error = false,
                                   bool as_system = false);

    /// Restore a session from the same storage root that produced its picker
    /// entry.  Kept in the app layer because command handlers do not own the
    /// adapter's explicit session-storage override.
    bool ResumeSessionFromDirectory(const std::filesystem::path& sessions_dir,
                                    std::string_view session_id,
                                    std::string_view status_message);


    // Run a user-initiated `!` command LOCALLY (never an LLM turn).  It runs
    // the command with the sandbox disabled, renders a <bash-input> user row
    // plus a <bash-stdout>/<bash-stderr> output row, and NEVER sends the
    // command to the model.
    //
    // Append the input row immediately, then run `/bin/sh -c` on a worker
    // thread
    // (combined stdout+stderr via popen_spawn, run in the session cwd) and post
    // the output back to the render thread via pending_bash_result_.  The
    // engine / query path is never touched, so no Bash *tool-use* card and no
    // assistant summary are produced — matching the transcript exactly.
    void RunLocalBashCommand(std::string command);

    void ClearActiveLocalJsxCommand();

    void DismissLocalJsxCommand(std::string result_message);

    // Static text/UTF helpers — bodies in app_helpers.cpp (RFC 0001
    // Phase C batch 1). The static lowercase_ascii is byte-identical to
    // the free function above; different scopes, no ODR issue.
    [[nodiscard]] static std::string lowercase_ascii(std::string_view value);

    void RefreshAutocompleteSuggestions();


    // Agents menu methods are defined in app_agent_menu.cpp so the
    // agent_cards closure stays out of this interface BMI.
    void RefreshAgentsMenuOutput();
    void LoadAgentCardsForMenu();
    void OpenAgentsMenu();
    void OpenTeamsOverview();
    bool HandleLocalJsxEvent(const Event& ev);

    // Skills-menu helpers — bodies in app_skills_menu.cpp (RFC 0001
    // Phase C batch 1).
    [[nodiscard]] static int skill_source_order(std::string_view source);
    [[nodiscard]] static bool is_visible_skills_menu_source(
        std::string_view source);

    // UTF-16 code-unit / rough-JS-token estimates — bodies in
    // app_helpers.cpp (RFC 0001 Phase C batch 1).
    [[nodiscard]] static bool utf8_continuation(unsigned char ch);
    [[nodiscard]] static std::size_t utf16_code_unit_count(
        std::string_view value);
    [[nodiscard]] static std::size_t rough_js_token_count(
        std::string_view value);

    // Skills-menu formatting + open — bodies in app_skills_menu.cpp
    // (RFC 0001 Phase C batch 1). OpenSkillsMenu mutates screen_state_ and
    // calls PostRenderEvent() (body in app_animation.cpp since batch 2).
    [[nodiscard]] static std::size_t skills_menu_token_estimate(
        const acsrc::SkillSuggestionData& skill);
    [[nodiscard]] static std::string collapse_home_path(std::string path);
    [[nodiscard]] static std::string skill_source_group_title(
        const acsrc::SkillSuggestionData& skill);
    [[nodiscard]] static std::string FormatSkillsMenuOutput(
        std::vector<acsrc::SkillSuggestionData> skills);
    void OpenSkillsMenu();

public:
    ~AppAdapter() override;

    void StartProgramStatus(bool interactive) noexcept;
    void FinishProgramStatus(bool normal) noexcept;

    AppAdapter(void* engine, void* lifecycle_hooks,
               void* cmd_registry,
               std::optional<std::filesystem::path> sessions_dir,
               std::function<void()> on_exit);

    void HandleSubmit(const std::string& text,
                      repl::InputMode submit_mode = repl::InputMode::Normal,
                      bool wait_for_pastes = true,
                      bool retry_request = false);

    void set_tool_round_budget(std::optional<std::uint32_t> budget);

    void HandleCommand(std::string_view cmd);

    void ProjectRuntimeMetadataToScreenState();

    /// Project the AI-managed todo list (TodoWriteTool singleton) into
    /// screen_state_->todo_store so the todo panel renders above the prompt.
    void ProjectTodosToScreenState();

    /// Project settings from SettingsManager into screen_state_.
    /// Projects the settings subset into the REPL
    /// screen's model/status-line fields.  Only the subset needed by the
    /// renderer is projected — the engine owns the full settings object.
    void ProjectSettingsToScreenState();

    /// RFC 0007 Phase 2: set the persistent mouse-mode hint in the status
    /// bar. Called once at startup from RunApp with the config-derived
    /// mouse_mode value.
    void set_mouse_mode_label(bool mouse_mode);

    /// Hybrid mouse mode: store whether FTXUI mouse tracking is active.
    /// When true, F2 toggles mouse tracking at runtime.
    void set_mouse_mode(bool active);

    /// Trigger an async statusline update (debounced).
    /// P3-1c: delegates to StatuslineCoordinator.  Kept as a thin wrapper
    /// because 5 impl units call it.
    /// Body in app_constructor.cpp (inline-def ratchet).
    void TriggerStatuslineUpdate();

    // Build the statusline JSON payload / execute the user command.
    // Out-of-line in app_constructor.cpp so loom.ui.app.statusline_runner,
    // loom.utils.model and loom.constants stay out of this interface's BMI.
    [[nodiscard]] std::string BuildStatuslineInputJson();
    bool ExecuteStatuslineCommand(std::string_view command,
                                  std::string json_input,
                                  int timeout_ms,
                                  std::string& output);

    // Apply a chain of collapse passes to the message list before projecting
    // rows:
    //   collapseBackgroundBashNotifications(collapseHookSummaries(
    //     collapseTeammateShutdowns(collapseReadSearchGroups(grouped, tools))))
    //
    // We run the same chain here, on the raw conversation, before the
    // per-message projection loop in SyncState()/Render().  Only the passes
    // that have a faithful port are wired so far:
    //   * collapseBackgroundBashNotifications — DONE (this call).
    //   * collapseHookSummaries / collapseTeammateShutdowns / collapseReadSearch
    //     — pending (need richer SystemMessage / AttachmentMessage types).
    // As each pass lands it slots in here, preserving the ordering.
    //
    // `fullscreen=true`: the CPP transcript is always the fullscreen-equivalent
    // view (collapse gated on isFullscreenEnvEnabled()).  `verbose=false`:
    // there is no ctrl+O verbose transcript toggle at this layer yet, so we use
    // the default collapsed presentation (each item shown only in verbose).
    [[nodiscard]] std::vector<Message> ApplyMessageCollapsePipeline(
        std::vector<Message> messages) const;

    void SyncState();

    // Rebuild screen_state_->task_view_store.live_teammates from the native agent store +
    // pane-observer snapshot. Defined in app_team_projection.cpp; callers own
    // the render wake.
    void ProjectLiveTeammatesToScreenState();
    [[nodiscard]] std::function<void()> MakeRuntimeAgentChangeCallback();
    void SubscribeRuntimeAgentChanges();
    void UnsubscribeRuntimeAgentChanges();
    void ProjectRuntimeAgentsToScreenState();
    void OpenAgentInstances(std::string_view argument);
    void ContinueAgentInstance(std::string session, std::string id, std::string prompt);
    void StopAgentInstance(std::string_view session, std::string_view id);

    // Leader-side: drain one queued teammate permission_request into the
    // existing ToolPermission dialog (Band3 overlay). Defined in
    // app_team_projection.cpp. Returns true when a dialog was pushed.
    bool drain_one_teammate_permission();

    void ConsumePendingResult();

    Element Render() override;

    bool OnEvent(Event event) override;

    Component ActiveChild() override;

    void set_screen(ScreenInteractive* screen) {
        screen_.store(screen, std::memory_order_release);
    }

    // ── Async clipboard paste worker ──────────────────────────────────────
    // Spawns a detached thread that reads the clipboard image.  On success
    // the ImageBlock is posted to pending_paste_results_; on failure the id
    // is posted to pending_paste_failures_.  ProcessCompletedPastes() drains
    // both queues on the render/event thread.
    //
    // Why async?  std::system() + osascript fork + PNG-to-file + base64
    // encode takes 100-500ms on macOS.  Doing that synchronously in OnEvent
    // blocks the FTXUI render loop, causing visible UI freeze and (worse)
    // terminal raw-mode state corruption that can take seconds to recover
    // from.  The placeholder "[Image #N]" is inserted synchronously so the
    // user gets instant feedback; the image data fills in shortly after.
    //
    // P4-1b: delegates to PasteCoordinator::SpawnPasteWorker with a
    // PostRenderEvent callback.
    void SpawnPasteWorker(int id);

    /// Drain background paste results onto pasted_contents_ (render thread).
    /// Called at the top of every OnEvent so results are picked up as soon as
    /// possible without blocking.  Failed pastes have their "[Image #N]"
    /// placeholder removed from input_text.  Text pastes replace "[Image #N]"
    /// with the actual text (truncating if >10K chars).
    void ProcessCompletedPastes();

    /// Check whether submission must resume on a clipboard completion event.
    bool HasInFlightPasteReferences(const std::string& text) const;
    void DrainCompletedPastesAndSubmit();

    [[nodiscard]] std::function<bool(std::string_view, std::string_view)> get_permission_callback();

private:
    // ── Teammate inbox test seams (bodies in the :team partition) ─────────
    void configure_teammate_for_testing(std::string agent_name, std::string team);
    void poll_teammate_inbox_once_for_testing();
    [[nodiscard]] std::size_t teammate_pending_count_for_testing();
    [[nodiscard]] std::string pop_teammate_prompt_for_testing();

    // ── Test seams ──────────────────────────────────────────────────────
    // Bodies in app_testing_seams.cpp (RFC 0002 F3 Finalize) — the inline
    // ratchet (inline_def_check.py) re-freezes at the single composition
    // body (set_screen). Private access holds within member functions.
    [[nodiscard]] bool is_query_running_for_testing() const noexcept;

    /// Inject a stream event directly into the App's event handler.
    /// Bypasses the query thread, HTTP, and wire layers. For testing only.
    void inject_stream_event_for_testing(const core::StreamEvent& ev);

    /// Set query_running_ for testing. When true, Render() projects streaming
    /// state. When false, the next Render() takes the idle path
    /// (ConsumePendingResult → SyncState), clearing streaming state. Also sets
    /// the spinner to Requesting (matching HandleSubmit) so the idle path fires
    /// on __end_query__ even for fixtures with no content events.
    void set_query_running_for_testing(bool running);

    /// Clear the streaming-thinking preview map so
    /// is_streaming_thinking_visible() returns false. For testing only —
    /// deactivates the in-flight projection path so the committed thinking
    /// row takes over immediately, letting the 3s collapse grace
    /// (was_recently_streaming in messages_list_payload_row.cpp) become
    /// the deciding factor (RFC 0003 §8.3).
    void clear_streaming_thinking_for_testing();

    // Drive a prompt submission through the full HandleSubmit path (slash /
    // bash / LLM routing) exactly as the Enter key would.
    void submit_for_testing(const std::string& text);

    // True while a local '!' bash command worker is still running.
    [[nodiscard]] bool is_local_bash_running_for_testing() const noexcept;

    // Block until the local '!' bash worker finishes, then drain its output
    // into the transcript (mirrors what the render loop does each frame).
    void wait_for_local_bash_for_testing();

    [[nodiscard]] bool is_loading_for_testing() const noexcept;

    [[nodiscard]] std::uint64_t ui_animation_tick_count_for_testing() const noexcept;

    [[nodiscard]] std::string status_message_for_testing() const;

    [[nodiscard]] bool status_line_enabled_for_testing() const noexcept;

    [[nodiscard]] std::string status_line_command_for_testing() const;

    [[nodiscard]] int status_line_padding_for_testing() const noexcept;

    [[nodiscard]] std::string status_bar_model_for_testing() const;

    [[nodiscard]] std::size_t autocomplete_suggestion_count_for_testing() const noexcept;

    [[nodiscard]] std::vector<std::string> autocomplete_suggestions_for_testing() const;

    [[nodiscard]] int autocomplete_index_for_testing() const noexcept;

    // Debug/testing: snapshot screen_state_->messages_store.messages as "label:preview" rows
    // to verify transcript ordering (local-command vs user vs assistant).
    [[nodiscard]] std::vector<std::string> messages_for_testing() const;

    [[nodiscard]] std::string input_text_for_testing() const;

    /// Number of entries in pasted_contents_ (for testing orphan cleanup).
    [[nodiscard]] std::size_t pasted_contents_size_for_testing() const noexcept;

    /// Check if a specific paste-id is still in pasted_contents_ (for testing
    /// orphan cleanup after placeholder deletion).
    [[nodiscard]] bool has_pasted_content_for_testing(int id) const noexcept;

    /// Inject a pasted image directly (bypasses clipboard read — for testing
    /// HandleSubmit's referenced-ids filter and empty-text+images guard).
    void inject_pasted_image_for_testing(int id, ImageBlock ib);

    /// When true, SpawnPasteWorker injects a tiny fake PNG synchronously into
    /// pending_paste_results_ instead of spawning a detached thread that reads
    /// the real clipboard. Lets tests exercise the Ctrl+V → placeholder →
    /// submit path without the lifetime hazard of a detached thread outliving
    /// the test's AppAdapter.
    bool no_real_paste_worker_for_testing_ = false;
    void set_no_real_paste_worker_for_testing(bool v);

    /// Set input_text directly (for testing orphan cleanup and submit guards
    /// without going through the text input component).
    void set_input_text_for_testing(std::string text);

    /// Set the next-action suggestion directly (for testing Tab-to-accept
    /// without running the full PromptSuggestionService).
    void set_next_action_suggestion_for_testing(std::string suggestion);

    /// Expose HandleSubmit for direct test invocation (the real submit path
    /// goes through the text input component's on_submit callback).
    void handle_submit_for_testing(std::string text);

    /// Run the orphan-cleanup logic against the current
    /// screen_state_->input_text.  For testing only.
    void trigger_orphan_cleanup_for_testing();

    [[nodiscard]] bool is_agents_view_for_testing() const noexcept;

    [[nodiscard]] bool is_local_jsx_command_for_testing(
        std::string_view command_name) const noexcept;

    [[nodiscard]] int active_agents_selection_position_for_testing() const noexcept;

    [[nodiscard]] std::size_t agent_card_count_for_testing() const noexcept;

    [[nodiscard]] bool has_pending_dialog_for_testing() const noexcept;

    // Out-of-line in the :team partition so the live_teammates /
    // dialogs.system closures stay out of this interface.
    void set_live_teammates_for_testing(void* v);
    [[nodiscard]] bool teams_overview_open_for_testing() const;

    [[nodiscard]] int teams_overview_count_for_testing() const noexcept;

    // Enqueue a stage-A permission_request as if the leader inbox poll found
    // it (exercises the ToolPermission dialog + PermissionSync reply path
    // without a real tmux worker mailbox).
    void enqueue_teammate_permission_for_testing(void* request, std::string team);
    [[nodiscard]] std::size_t pending_teammate_permission_count_for_testing();

    // RFC 0007: text selection state for threshold/unit tests.
    [[nodiscard]] bool text_selection_active_for_testing() const noexcept;
    [[nodiscard]] bool text_selection_potential_for_testing() const noexcept;
    [[nodiscard]] bool text_selection_copied_for_testing() const noexcept;
    [[nodiscard]] std::size_t text_selection_copied_chars_for_testing() const noexcept;
    /// Simulate a completed copy: sets active + copied + copied_chars.
    void simulate_copy_for_testing(std::size_t chars);
    /// F2 toggle: current mouse tracking state.
    [[nodiscard]] bool mouse_tracking_on_for_testing() const noexcept;

    // RFC 0007: copy result pipeline for stale-result/OSC 52 tests.
    void inject_copy_result_for_testing(
        std::uint64_t generation,
        loom::utils::clipboard::CopyResult result,
        std::string text);
    void drain_copy_result_for_testing();
    [[nodiscard]] std::string footer_notification_text_for_testing() const;
    void set_copy_generation_for_testing(std::uint64_t gen);

    // ── Virtual renderer hysteresis (RFC 0008) ─────────────────────
    void set_virtual_renderer_state_for_testing(
        bool is_virtual, std::uint64_t switch_count, bool force_bottom);
    [[nodiscard]] bool virtual_renderer_is_virtual_for_testing() const noexcept;
    [[nodiscard]] std::uint64_t
    virtual_mode_switch_count_for_testing() const noexcept;
    [[nodiscard]] bool
    force_bottom_after_mode_switch_for_testing() const noexcept;
    /// Trigger the on_clear_session callback (session-expired card path).
    void trigger_clear_session_for_testing();
};

// ============================================================
// P5: Testing seam accessor
// ============================================================
// AppTestingSeams is the ONLY caller of AppAdapter's private *_for_testing
// methods (granted via friend declaration above).  Production code never
// uses this struct — it exists so tests can drive the app without exposing
// 38 testing methods on AppAdapter's public API.
//
// Bodies live in app_testing_seams.cpp (inline-def ratchet); the struct
// stays here because tests construct it via the test_seams() factories.
//
// Usage in tests:
//   test_seams(app).submit_for_testing("hello");
//   EXPECT_TRUE(test_seams(app).is_query_running_for_testing());

struct AppTestingSeams {
    AppAdapter* app;

    // Exercise production geometry on deterministic rendered cells.
    [[nodiscard]] static std::string selected_text_for_testing(
        const ftxui::Screen& screen, int x0, int y0, int x1, int y1);
    [[nodiscard]] static ftxui::Element selection_highlight_for_testing(
        ftxui::Element child, int x0, int y0, int x1, int y1);
    [[nodiscard]] std::optional<std::string> copy_feedback_for_testing() const;

    // UI-thread-only inspection; workers must never mutate the returned state.
    [[nodiscard]] std::shared_ptr<const repl::ReplScreenState> screen_state_for_testing() const;
    [[nodiscard]] std::shared_ptr<repl::ReplScreenState> agent_conversation_state_for_testing() const;
    [[nodiscard]] std::string active_agent_conversation_for_testing() const;
    [[nodiscard]] bool stop_background_workers_for_testing();
    [[nodiscard]] std::size_t pending_ui_action_count_for_testing() const;
    [[nodiscard]] std::function<void()> runtime_agent_change_callback_for_testing();
    [[nodiscard]] std::shared_ptr<std::atomic<bool>> runtime_agent_dirty_for_testing() const;
    [[nodiscard]] std::function<void(std::function<void()>)> ui_dispatcher_for_testing();
    [[nodiscard]] bool resume_session_for_testing(
        const std::filesystem::path& sessions_dir, std::string session_id);
    void start_program_status_for_testing(std::function<bool(std::string_view)> writer);
    void enqueue_program_status_completion_for_testing(bool error, bool paused = false);

    // ── Teammate inbox seams ──────────────────────────────────────
    void configure_teammate_for_testing(std::string agent_name, std::string team);
    void poll_teammate_inbox_once_for_testing();
    [[nodiscard]] std::size_t teammate_pending_count_for_testing();
    [[nodiscard]] std::string pop_teammate_prompt_for_testing();

    // ── Query / loading state ──────────────────────────────────────
    [[nodiscard]] bool is_query_running_for_testing() const noexcept;
    void submit_for_testing(const std::string& text);
    [[nodiscard]] bool is_local_bash_running_for_testing() const noexcept;
    void wait_for_local_bash_for_testing();
    [[nodiscard]] bool is_loading_for_testing() const noexcept;
    void inject_stream_event_for_testing(const loom::core::StreamEvent& ev);
    void set_query_running_for_testing(bool running);
    void notify_conversation_changed_for_testing();
    void clear_streaming_thinking_for_testing();

    // ── Animation / statusline ─────────────────────────────────────
    [[nodiscard]] std::uint64_t ui_animation_tick_count_for_testing() const noexcept;
    [[nodiscard]] std::string status_message_for_testing() const;
    [[nodiscard]] bool status_line_enabled_for_testing() const noexcept;
    [[nodiscard]] std::string status_line_command_for_testing() const;
    [[nodiscard]] int status_line_padding_for_testing() const noexcept;
    [[nodiscard]] std::string status_bar_model_for_testing() const;

    // ── Autocomplete ───────────────────────────────────────────────
    [[nodiscard]] std::size_t autocomplete_suggestion_count_for_testing() const noexcept;
    [[nodiscard]] std::vector<std::string> autocomplete_suggestions_for_testing() const;
    [[nodiscard]] int autocomplete_index_for_testing() const noexcept;

    // ── Messages / input ───────────────────────────────────────────
    [[nodiscard]] std::vector<std::string> messages_for_testing() const;
    [[nodiscard]] std::string input_text_for_testing() const;
    /// Test-only: skip chain compression in the messages list.
    void set_disable_chain_compression_for_testing(bool v);

    // ── Paste ──────────────────────────────────────────────────────
    [[nodiscard]] std::size_t pasted_contents_size_for_testing() const noexcept;
    [[nodiscard]] bool has_pasted_content_for_testing(int id) const noexcept;
    void inject_pasted_image_for_testing(int id, loom::core::ImageBlock ib);
    void set_no_real_paste_worker_for_testing(bool v);

    // ── Input / submit ─────────────────────────────────────────────
    void set_input_text_for_testing(std::string text);
    void set_next_action_suggestion_for_testing(std::string suggestion);
    void handle_submit_for_testing(std::string text);
    void trigger_orphan_cleanup_for_testing();

    // ── Agents view ────────────────────────────────────────────────
    [[nodiscard]] bool is_agents_view_for_testing() const noexcept;
    [[nodiscard]] bool is_local_jsx_command_for_testing(
        std::string_view command_name) const noexcept;
    [[nodiscard]] int active_agents_selection_position_for_testing() const noexcept;
    [[nodiscard]] std::size_t agent_card_count_for_testing() const noexcept;

    // ── Dialogs / teams ────────────────────────────────────────────
    [[nodiscard]] bool has_pending_dialog_for_testing() const noexcept;
    void set_live_teammates_for_testing(void* v);
    [[nodiscard]] bool teams_overview_open_for_testing() const;
    [[nodiscard]] int teams_overview_count_for_testing() const noexcept;

    // ── Teammate permissions ───────────────────────────────────────
    void enqueue_teammate_permission_for_testing(void* request, std::string team);
    [[nodiscard]] std::size_t pending_teammate_permission_count_for_testing();

    // ── Text selection (RFC 0007) ──────────────────────────────────
    [[nodiscard]] bool text_selection_active_for_testing() const noexcept;
    [[nodiscard]] bool text_selection_potential_for_testing() const noexcept;
    [[nodiscard]] bool text_selection_copied_for_testing() const noexcept;
    [[nodiscard]] std::size_t text_selection_copied_chars_for_testing() const noexcept;
    /// Simulate a completed copy: sets active + copied + copied_chars.
    void simulate_copy_for_testing(std::size_t chars);
    /// F2 toggle: current mouse tracking state.
    [[nodiscard]] bool mouse_tracking_on_for_testing() const noexcept;

    // ── Copy result pipeline (RFC 0007) ────────────────────────────
    /// Inject a pending copy result as if the worker thread posted it.
    void inject_copy_result_for_testing(
        std::uint64_t generation,
        loom::utils::clipboard::CopyResult result,
        std::string text);
    /// Drain pending copy results on the UI thread.
    void drain_copy_result_for_testing();
    /// Get the current footer notification text (empty if none).
    [[nodiscard]] std::string footer_notification_text_for_testing() const;
    /// Set the copy generation counter (for stale-result discard tests).
    void set_copy_generation_for_testing(std::uint64_t gen);

    // ── Virtual renderer hysteresis (RFC 0008) ─────────────────────
    /// Set the persistent virtual renderer state (mode, counter, flag).
    void set_virtual_renderer_state_for_testing(
        bool is_virtual, std::uint64_t switch_count,
        bool force_bottom);
    /// True when the renderer is in Virtual mode.
    [[nodiscard]] bool virtual_renderer_is_virtual_for_testing() const noexcept;
    /// Number of Static↔Virtual mode switches since last reset.
    [[nodiscard]] std::uint64_t
    virtual_mode_switch_count_for_testing() const noexcept;
    /// Persistent bottom-anchor flag after a mode-switch scroll reset.
    [[nodiscard]] bool
    force_bottom_after_mode_switch_for_testing() const noexcept;
    /// Trigger the on_clear_session callback (session-expired card path).
    void trigger_clear_session_for_testing();
};

/// Factory: create an AppTestingSeams proxy from a Component (shared_ptr
/// to ComponentBase — dynamic_casts to AppAdapter).  Accepts const& so
/// shared_ptr<AppAdapter> temporaries bind via the implicit conversion.
[[nodiscard]] AppTestingSeams test_seams(const ftxui::Component& app);

/// Factory: create an AppTestingSeams proxy from an AppAdapter pointer.
[[nodiscard]] AppTestingSeams test_seams(AppAdapter* app);

// ============================================================
// Main Application Runner
// ============================================================

} // namespace loom::ui
