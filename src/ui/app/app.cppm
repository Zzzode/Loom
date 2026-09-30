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

export module cc.ui.app.app;

import std;

import cc.types.types;
import cc.ui.visual.markdown;
import cc.ui.screens.repl_state;
import cc.ui.prompt.autocomplete_sources;
// P0-2: 7-stage message pipeline utilities (dedup / tag filter / tool augment).
import cc.ui.messages.message_pipeline;
import cc.ui.dialogs.system;

export namespace cc::ui {

// PIMPL backing type, defined in the internal partition cc.ui.app.app:impl.
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

// Nested PIMPL for the teammate inbox/permission cluster. The backing struct
// lives entirely in app_team.cpp (a plain impl unit), keeping swarm_helpers /
// team_helpers / swarm_backends out of both this interface and the :impl
// partition.
struct TeammateState;
struct TeammateStateDeleter {
    void operator()(TeammateState* p) const noexcept;
};

// Nested PIMPL for settings (disk load + file-watch). Defined in
// app_settings.cpp; keeps cc.utils.settings_manager out of the :impl BMI.
struct SettingsState;
struct SettingsStateDeleter {
    void operator()(SettingsState* p) const noexcept;
};

// Defined in app_store_bridge.cpp; type-erased shared_ptr factory so :impl
// never imports cc.state.{store,app_state}.
[[nodiscard]] std::shared_ptr<void> create_typed_app_store();

// Plain-data projection of the AppState bridge fields, returned by
// AppAdapter::bridge_state() so callers need not import cc.state.app_state.
struct BridgeState {
    bool enabled = false;
    bool explicit_remote = false;
    bool connected = false;
    bool session_active = false;
    bool reconnecting = false;
};

// SL-11: defined in app_prompt_suggestion_wiring.cpp (impl unit) to keep the
// heavy cc.services.prompt_suggestion import out of this thin module (clang
// 2GB source-location budget).
void wire_prompt_suggestion_hook(void* hooks, void* engine,
                                 std::shared_ptr<cc::ui::repl_screen::ReplScreenState> state);

using namespace ftxui;
using namespace cc::core;

namespace repl = cc::ui::repl_screen;
namespace acsrc = cc::ui::autocomplete_sources;

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
// ranking now delegates to cc::ui::prompt::fuzzy_rank_nucleo (frn::), which
// ports the nucleo/fzf-v2 scorer (boundary/camel/consecutive/gap/path bonuses)
// while preserving the exact {0..3} base range so the tier offsets (alias +1,
// skill +4, plugin +6) and the rank-ascending sort stay unchanged. See
// ui/prompt/fuzzy_rank_nucleo.cppm. lowercase_ascii() above is retained.

// ============================================================
// Projection: Engine state -> ReplScreenState
// ============================================================

[[nodiscard]] repl::MessageDisplayEntry project_message(const Message& msg);

// ============================================================
// project_messages — TS-faithful projection that splits a single
// AssistantMessage into MULTIPLE display rows when it mixes a ThinkingBlock
// with a TextBlock / ToolUseBlock.  TS renders these as separate sibling
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

// ============================================================
// Convenience: render a single core Message to an Element.
// Used by tests and callers that want a quick rendering of one message.
// ============================================================

[[nodiscard]] Element RenderMessage(const Message& msg);

// ============================================================
// App Adapter Component
// ============================================================

class AppAdapter : public ComponentBase {
private:
    std::unique_ptr<AppImpl, AppImplDeleter> impl_;
    std::unique_ptr<TeammateState, TeammateStateDeleter> teammate_;
    std::unique_ptr<SettingsState, SettingsStateDeleter> settings_;
    // Defined in the :impl partition where AppImpl is complete. The out-of-line
    // constructor body calls this; teardown goes through AppImplDeleter, so
    // neither impl unit needs AppImpl's layout.
    // Stored type-erased as void* in AppImpl; each impl unit casts back
    // after importing the owning module (cc.query/cc.hooks/cc.commands/
    // cc.utils.session_storage), keeping those closures out of this BMI.
    void construct_impl(void* engine, void* lifecycle_hooks,
                        void* cmd_registry, void* storage);
    void construct_teammate();
    void construct_settings();
    [[nodiscard]] void* engine_raw() const noexcept;
    [[nodiscard]] void* lifecycle_hooks_raw() const noexcept;
    [[nodiscard]] void* cmd_registry_raw() const noexcept;
    [[nodiscard]] void* storage_raw() const noexcept;

    std::function<void()> on_exit_;

    std::shared_ptr<repl::ReplScreenState> screen_state_;
    Component repl_component_;
    std::vector<repl::MessageDisplayEntry> local_command_messages_;

    std::string current_session_id_;

    // Ctrl-C double-press ExitHandler moved into AppImpl (:impl partition);
    // access via set_exit_message_impl/reset_exit_handler/handle_ctrl_c.

    // Session start time for duration tracking (statusline cost.total_duration_ms)
    std::chrono::steady_clock::time_point session_start_time_;

    // Async query state
    std::jthread query_thread_;
    std::jthread spinner_thread_;
    // Local '!' bash command worker (TS processBashCommand.tsx). Runs the
    // command outside the LLM turn (shouldQuery:false), so it uses its own
    // thread rather than query_thread_ and never sets query_running_.
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
    /// TS REF: SystemAPIErrorMessage.tsx onRetry → re-submits last prompt.
    std::string last_submitted_text_;
    // Cached autocomplete data (loaded once at startup to avoid repeated disk I/O
    // on every keystroke — TS memoizes these in useTypeahead).
    std::vector<acsrc::SkillSuggestionData> cached_skills_;
    std::vector<acsrc::PluginCommandSuggestionData> cached_plugin_commands_;
    std::atomic<std::uint64_t> ui_animation_tick_count_{0};
    std::mutex result_mutex_;
    std::optional<std::string> pending_error_;
    // Pasted clipboard images keyed by paste-id (TS pastedContents: Record).
    // Each ctrl+v image paste assigns a monotonically-increasing id and
    // inserts "[Image #N]" into input_text at the cursor.  Orphan cleanup
    // (see OnEvent + HandleSubmit) prunes entries whose placeholder is no
    // longer in the text.  TS REF: PromptInput.tsx L144 + L1151-1200.
    std::unordered_map<int, ImageBlock> pasted_contents_;
    // Pasted clipboard TEXT content keyed by paste-id.  When a >10K char
    // text paste is truncated, the middle (elided) content is stored here
    // so that HandleSubmit can expand [...Truncated text #N] refs back to
    // the full text before sending to the model.
    // TS REF: inputPaste.ts maybeTruncateInput — stores {id, type: 'text',
    //   content: placeholderContent} in pastedContents.
    std::unordered_map<int, std::string> pasted_text_contents_;
    int next_paste_id_ = 1;
    // Async paste: the placeholder "[Image #N]" is inserted into input_text
    // immediately on Ctrl+v (instant UI feedback), and the actual clipboard
    // read (osascript + PNG encode) runs on a background thread.  Results
    // are posted back via these queues and drained on the next OnEvent.
    std::mutex paste_mutex_;
    std::unordered_map<int, ImageBlock> pending_paste_results_;  // bg→UI
    std::unordered_set<int> pending_paste_failures_;             // bg→UI
    // Async text paste results: raw clipboard text keyed by paste-id.
    // Posted by SpawnPasteWorker when the clipboard has no image but does
    // have text.  ProcessCompletedPastes replaces the "[Image #N]"
    // placeholder with the text (truncating if >10K).
    std::unordered_map<int, std::string> pending_paste_text_results_;  // bg→UI
    // Track ids whose SpawnPasteWorker thread is still in flight (read hasn't
    // posted to pending_paste_results_/failures_ yet). HandleSubmit waits on
    // these so a fast Ctrl+V→Enter doesn't submit before the image data lands
    // (which would send a text-only message with [Image #N] refs but no PNG).
    std::unordered_set<int> in_flight_pastes_;
    // Local '!' bash command output posted back from bash_thread_ (bg→UI),
    // drained on the render thread in ConsumePendingResult().  Mirrors the
    // pending_paste_results_ handoff pattern so local_command_messages_ /
    // SyncState are only ever mutated on the render thread.
    std::mutex bash_result_mutex_;
    struct PendingBashResult {
        std::string output;   // combined stdout+stderr (already trimmed)
        bool is_error = false;
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
    /// TS REF: Markdown.tsx L186-235 — StreamingMarkdown stable-prefix cache
    /// for the streaming-text tail row.  Reset alongside streaming_text_ so
    /// each new model response starts with a fresh stable prefix.  Used by
    /// RenderAssistantTextMessageFaithful via MessagesListInput.streaming_md.
    ::cc::ui::StreamingMarkdown streaming_markdown_;
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
        bool exec_done = false;      ///< ToolExecutionEnd: tool has finished executing
        bool is_error = false;
    };
    // TS REF: src/utils/messages.ts L2921-2925  StreamingThinking type
    //   { thinking, isStreaming, streamingEndedAt }
    // streaming_ended_at enables the 30s grace period after thinking stops
    // (TS REF: Messages.tsx L382-389  isStreamingThinkingVisible).
    struct StreamingThinkingPreview {
        std::string text;
        bool complete = false;
        std::optional<std::chrono::steady_clock::time_point> streaming_ended_at;
    };
    std::map<std::uint32_t, StreamingToolPreview> streaming_tools_;
    std::map<std::uint32_t, StreamingThinkingPreview> streaming_thinking_;

    // TS REF: Messages.tsx L382-389  isStreamingThinkingVisible useMemo.
    // Returns true when any streaming thinking block is still being streamed,
    // OR when a recently-completed thinking block is within the 30-second
    // grace period (TS: Date.now() - streamingEndedAt < 30000).
    // Drives G3 (hide all completed thinking when streaming visible) and
    // keeps the tail visible after ContentBlockStop fires.
    // Body in app_autocomplete.cpp (RFC 0001 Phase C batch 2).
    bool is_streaming_thinking_visible() const;
    // P0-2 Stage 1: per-turn dedup tracker for ContentBlock index transitions.
    // One per App (one instantiation per repl lifetime; cleared on each turn start.
    cc::ui::messages::pipeline::DedupTracker event_dedup_;
    std::atomic<ScreenInteractive*> screen_{nullptr};

    // Permission confirmation
    std::mutex permission_mutex_;
    std::condition_variable permission_cv_;
    std::optional<bool> permission_response_;
    std::set<std::string> always_allowed_tools_;
    /// Whether any computer-use action has been approved/asked this session —
    /// drives the "first computer use in this session" warning in the panel.
    bool computer_use_seen_in_session_ = false;

    // MCP Elicitation (synchronous dialog response pattern,
    // same as tool permission — blocks worker thread on UI response).
    std::mutex elicitation_mutex_;
    std::condition_variable elicitation_cv_;
    std::optional<bool> elicitation_response_;

    // Ask-user prompt (same synchronous dialog response pattern).
    // Used by the ask_user_question tool to show a PromptDialog instead
    // of falling back to stdio.
    std::mutex ask_user_mutex_;
    std::condition_variable ask_user_cv_;
    std::optional<std::optional<std::string>> ask_user_response_;

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

    // Statusline runner — async execution of user-configurable shell command.
    // Triggered on mount, after messages change, and when settings change.
    // Faithful to TS StatusLine.tsx's debounced doUpdate() pattern.
    std::jthread statusline_thread_;
    std::atomic<bool> statusline_dirty_{false};
    std::atomic<bool> statusline_running_{false};
    std::mutex statusline_mutex_;
    std::condition_variable statusline_cv_;
    int statusline_debounce_ms_ = 300;  // TS: 300ms debounce
    // Memo cache: skip re-exec when command + JSON input are identical and
    // last run was < 30s ago.  Matches TS StatusLine.tsx memo dependency tuple
    // (lastAssistantMessageId, permissionMode, vimMode, mainLoopModel).
    std::string statusline_last_cmd_;
    std::string statusline_last_input_json_;
    std::chrono::steady_clock::time_point statusline_last_run_{};
    // P0-6 builtin statusline: git branch detection cache.  We only re-run
    // `git rev-parse --abbrev-ref HEAD` when the cwd changes (cd events are
    // rare).  This avoids spawning a subprocess on every render tick.
    std::string last_branch_cwd_;
    std::string cached_git_branch_;

    // Animation ticker — body in app_animation.cpp (RFC 0001 Phase C
    // batch 2). The jthread lambda moves verbatim: same `this` capture,
    // same stop_token loop, same TriggerStatuslineUpdate() call.
    void StartUiAnimationTicker();

    // Post(Event::Custom) helper — body in app_animation.cpp (RFC 0001
    // Phase C batch 2). Private member; callable from member functions in
    // any impl unit (private access holds within member functions).
    void PostRenderEvent();

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

    void AppendLocalCommandMessage(std::string message, bool is_error = false);


    // TS REF: src/utils/processUserInput/processBashCommand.tsx
    //
    // Run a user-initiated `!` command LOCALLY (never an LLM turn).  TS does
    // BashTool.call({command, dangerouslyDisableSandbox:true}) with
    // shouldQuery:false, renders a <bash-input> user row plus a <bash-stdout>/
    // <bash-stderr> output row, and NEVER sends the command to the model.
    //
    // We mirror that: append the input row immediately (like TS's initial
    // setToolJSX(<BashModeProgress>)), then run `/bin/sh -c` on a worker thread
    // (combined stdout+stderr via popen_spawn, run in the session cwd) and post
    // the output back to the render thread via pending_bash_result_.  The
    // engine / query path is never touched, so no Bash *tool-use* card and no
    // assistant summary are produced — matching the TS transcript exactly.
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

    AppAdapter(void* engine, void* lifecycle_hooks,
               void* cmd_registry, void* storage,
               std::function<void()> on_exit);

    void HandleSubmit(const std::string& text,
                      repl::InputMode submit_mode = repl::InputMode::Normal);

    void HandleCommand(std::string_view cmd);

    void ProjectRuntimeMetadataToScreenState();

    /// Project settings from SettingsManager into screen_state_.
    /// Mirrors how the TS engine projects AppState.settings into the REPL
    /// screen's model/status-line fields.  Only the subset needed by the
    /// renderer is projected — the engine owns the full settings object.
    void ProjectSettingsToScreenState();

    /// Trigger an async statusline update (debounced).
    /// Faithful to TS scheduleUpdate() — sets a dirty flag and wakes the
    /// worker thread; the actual command runs after the debounce period.
    /// Body in app_constructor.cpp (RFC 0001 Phase C batch 2).
    void TriggerStatuslineUpdate();

    // Build the statusline JSON payload / execute the user command.
    // Out-of-line in app_constructor.cpp so cc.utils.statusline_runner,
    // cc.utils.model and cc.constants stay out of this interface's BMI.
    [[nodiscard]] std::string BuildStatuslineInputJson();
    bool ExecuteStatuslineCommand(std::string_view command,
                                  std::string json_input,
                                  int timeout_ms,
                                  std::string& output);

    // TS REF: src/components/Messages.tsx L519-520 — the render `useMemo`
    // applies a chain of collapse passes to the message list before projecting
    // rows:
    //   collapseBackgroundBashNotifications(collapseHookSummaries(
    //     collapseTeammateShutdowns(collapseReadSearchGroups(grouped, tools))))
    //
    // We run the same chain here, on the raw conversation, before the
    // per-message projection loop in SyncState()/Render().  Only the passes
    // that have a faithful CPP port are wired so far:
    //   * collapseBackgroundBashNotifications — DONE (this call).
    //   * collapseHookSummaries / collapseTeammateShutdowns / collapseReadSearch
    //     — pending (need richer SystemMessage / AttachmentMessage types).
    // As each pass lands it slots in here, preserving the TS ordering.
    //
    // `fullscreen=true`: the CPP transcript is always the fullscreen-equivalent
    // view (TS gates collapse on isFullscreenEnvEnabled()).  `verbose=false`:
    // there is no ctrl+O verbose transcript toggle at this layer yet, so we use
    // the default collapsed presentation (TS shows each item only in verbose).
    [[nodiscard]] std::vector<Message> ApplyMessageCollapsePipeline(
        std::vector<Message> messages) const;

    void SyncState();

    // Rebuild screen_state_->task_view_store.live_teammates from the native agent store +
    // pane-observer snapshot. Defined in app_team_projection.cpp; callers own
    // the render wake.
    void ProjectLiveTeammatesToScreenState();

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
    void SpawnPasteWorker(int id);

    /// Drain background paste results onto pasted_contents_ (render thread).
    /// Called at the top of every OnEvent so results are picked up as soon as
    /// possible without blocking.  Failed pastes have their "[Image #N]"
    /// placeholder removed from input_text.  Text pastes replace "[Image #N]"
    /// with the actual text (truncating if >10K chars).
    void ProcessCompletedPastes();

    /// Block (main thread, brief) until every [Image #N] referenced in `text`
    /// that still has an in-flight paste worker has either landed in
    /// pasted_contents_ / pending_paste_results_ / pending_paste_failures_.
    /// This closes the Ctrl+V→Enter race where a fast submit would snapshot
    /// pasted_contents_ before the PNG data arrived.
    ///
    /// Bounded wait (default ~3s) so a stuck/leaked worker never wedges the UI.
    /// Drains completed results on each tick so pasted_contents_ is fresh when
    /// HandleSubmit reads it immediately after this returns.
    void WaitForInFlightPastes(const std::string& text);

    // ── Teammate inbox test seams (bodies in the :team partition) ─────────
    void configure_teammate_for_testing(std::string agent_name, std::string team);
    void poll_teammate_inbox_once_for_testing();
    [[nodiscard]] std::size_t teammate_pending_count_for_testing();
    [[nodiscard]] std::string pop_teammate_prompt_for_testing();

    [[nodiscard]] std::function<bool(std::string_view, std::string_view)> get_permission_callback();

    [[nodiscard]] bool is_query_running_for_testing() const noexcept {        return query_running_.load();
    }

    // Drive a prompt submission through the full HandleSubmit path (slash /
    // bash / LLM routing) exactly as the Enter key would.
    void submit_for_testing(const std::string& text) {
        this->HandleSubmit(text);
    }

    // True while a local '!' bash command worker is still running.
    [[nodiscard]] bool is_local_bash_running_for_testing() const noexcept {
        return bash_running_.load();
    }

    // Block until the local '!' bash worker finishes, then drain its output
    // into the transcript (mirrors what the render loop does each frame).
    void wait_for_local_bash_for_testing() {
        if (bash_thread_.joinable()) bash_thread_.join();
        this->ConsumePendingResult();
    }

    [[nodiscard]] bool is_loading_for_testing() const noexcept {
        return screen_state_->task_view_store.spinner_mode != repl::SpinnerMode::Hidden;
    }

    [[nodiscard]] std::uint64_t ui_animation_tick_count_for_testing() const noexcept {
        return ui_animation_tick_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::string status_message_for_testing() const {
        return screen_state_->task_view_store.spinner_tip.value_or(std::string{});
    }

    [[nodiscard]] bool status_line_enabled_for_testing() const noexcept {
        return screen_state_->status_line_enabled;
    }

    [[nodiscard]] std::string status_line_command_for_testing() const {
        return screen_state_->status_line_command;
    }

    [[nodiscard]] int status_line_padding_for_testing() const noexcept {
        return screen_state_->status_line_padding;
    }

    [[nodiscard]] std::string status_bar_model_for_testing() const {
        return screen_state_->status_bar.model_name;
    }

    [[nodiscard]] std::size_t autocomplete_suggestion_count_for_testing() const noexcept {
        return screen_state_->autocomplete_suggestions.size();
    }

    [[nodiscard]] std::vector<std::string> autocomplete_suggestions_for_testing() const {
        std::vector<std::string> out;
        out.reserve(screen_state_->autocomplete_suggestions.size());
        for (const auto& suggestion : screen_state_->autocomplete_suggestions) {
            out.push_back(suggestion.display_text);
        }
        return out;
    }

    [[nodiscard]] int autocomplete_index_for_testing() const noexcept {
        return screen_state_->autocomplete_index;
    }

    // Debug/testing: snapshot screen_state_->messages_store.messages as "label:preview" rows
    // to verify transcript ordering (local-command vs user vs assistant).
    [[nodiscard]] std::vector<std::string> messages_for_testing() const {
        std::vector<std::string> out;
        out.reserve(screen_state_->messages_store.messages.size());
        for (const auto& m : screen_state_->messages_store.messages) {
            std::string label = m.role;
            if (m.is_local_command_input) label = "lc-input";
            else if (m.is_local_command_output) label = "lc-output";
            else if (m.is_thinking) label = "thinking";
            std::string pv = m.content_preview.substr(
                0, std::min<std::size_t>(30, m.content_preview.size()));
            out.push_back(label + ":" + pv);
        }
        return out;
    }

    [[nodiscard]] std::string input_text_for_testing() const {
        return screen_state_->input_text;
    }

    /// Number of entries in pasted_contents_ (for testing orphan cleanup).
    [[nodiscard]] std::size_t pasted_contents_size_for_testing() const noexcept {
        return pasted_contents_.size();
    }

    /// Check if a specific paste-id is still in pasted_contents_ (for testing
    /// orphan cleanup after placeholder deletion).
    [[nodiscard]] bool has_pasted_content_for_testing(int id) const noexcept {
        return pasted_contents_.contains(id);
    }

    /// Inject a pasted image directly (bypasses clipboard read — for testing
    /// HandleSubmit's referenced-ids filter and empty-text+images guard).
    void inject_pasted_image_for_testing(int id, ImageBlock ib) {
        pasted_contents_[id] = std::move(ib);
    }

    /// When true, SpawnPasteWorker injects a tiny fake PNG synchronously into
    /// pending_paste_results_ instead of spawning a detached thread that reads
    /// the real clipboard. Lets tests exercise the Ctrl+V → placeholder →
    /// submit path without the lifetime hazard of a detached thread outliving
    /// the test's AppAdapter.
    bool no_real_paste_worker_for_testing_ = false;
    void set_no_real_paste_worker_for_testing(bool v) {
        no_real_paste_worker_for_testing_ = v;
    }

    /// Set input_text directly (for testing orphan cleanup and submit guards
    /// without going through the text input component).
    void set_input_text_for_testing(std::string text) {
        screen_state_->input_text = std::move(text);
        screen_state_->input_cursor = screen_state_->input_text.size();
    }

    /// Expose HandleSubmit for direct test invocation (the real submit path
    /// goes through the text input component's on_submit callback).
    void handle_submit_for_testing(std::string text) {
        this->HandleSubmit(text);
    }

    /// Run the orphan-cleanup logic (TS PromptInput.tsx L1185-1200 useEffect)
    /// against the current screen_state_->input_text.  For testing only.
    void trigger_orphan_cleanup_for_testing();

    [[nodiscard]] bool is_agents_view_for_testing() const noexcept {
        return screen_state_->mode == repl::ReplMode::AgentsView;
    }

    [[nodiscard]] bool is_local_jsx_command_for_testing(
        std::string_view command_name) const noexcept {
        return screen_state_->active_local_jsx_command &&
               screen_state_->active_local_jsx_command_name == command_name;
    }

    [[nodiscard]] int active_agents_selection_position_for_testing() const noexcept {
        return screen_state_->active_agents_selection_position;
    }

    [[nodiscard]] std::size_t agent_card_count_for_testing() const noexcept {
        return screen_state_->task_view_store.agent_cards.size();
    }

    [[nodiscard]] bool has_pending_dialog_for_testing() const noexcept {
        return screen_state_->dialog_store.dialog_queue.has_overlay() ||
               screen_state_->dialog_store.dialog_queue.has_any_bottom() ||
               screen_state_->dialog_store.dialog_queue.has_modal() ||
               screen_state_->dialog_store.dialog_queue.has_standalone();
    }

    // Out-of-line in the :team partition so the live_teammates /
    // dialogs.system closures stay out of this interface.
    void set_live_teammates_for_testing(void* v);
    [[nodiscard]] bool teams_overview_open_for_testing() const;

    [[nodiscard]] int teams_overview_count_for_testing() const {
        return static_cast<int>(screen_state_->task_view_store.live_teammates.size());
    }

    // Enqueue a stage-A permission_request as if the leader inbox poll found
    // it (exercises the ToolPermission dialog + PermissionSync reply path
    // without a real tmux worker mailbox).
    void enqueue_teammate_permission_for_testing(void* request, std::string team);
    [[nodiscard]] std::size_t pending_teammate_permission_count_for_testing();
};

// ============================================================
// Main Application Runner
// ============================================================

} // namespace cc::ui
