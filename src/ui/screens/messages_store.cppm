// messages_store.cppm — RFC 0002 F3 store: message-list, scroll and
// transcript-chrome state, sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's loom.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). messages (7) and chrome (3) are downward-legal, so the
// concrete by-value field types (UnseenDivider, JumpHandle/VirtualListState,
// StickyPrompt) recreate no up-edge.
module;

export module loom.ui.screens.messages_store;

import std;

// Genuinely used (ImageBlock / ToolResultContentItem fields below), but only
// via leading-`::` qualified names `::loom::core::ImageBlock`, which the
// dead-import detector's prefix-chain cannot resolve — same blind spot as
// repl_screen.cppm's identical import, hence the keep-import marker.
import loom.types.types;                  // arch-check: keep-import
import loom.ui.messages.messages_list;    // UnseenDivider
import loom.ui.messages.virtual_list;     // JumpHandle / VirtualListState
import loom.ui.chrome.fullscreen_layout;  // StickyPrompt

export namespace loom::ui::repl_screen {

/// Minimal Message projection for orchestration.
/// Per-row rendering delegated to message_row.cppm (UI4/UI5).
/// Moved here from repl_state.cppm in RFC 0002 F3 (MessagesStore shard).
struct MessageDisplayEntry {
    std::string id, role, content_preview;
    /// Full untruncated content for thinking entries.  content_preview is
    /// truncated to 200 chars for search/display, but the thinking renderer
    /// needs the full text — populate this alongside content_preview for
    /// thinking rows.
    std::string full_content;
    bool is_streaming = false, is_thinking = false, is_tool_use = false;
    /// True when the thinking block is still being streamed (or the entry
    /// is a static projection that should render the collapsed "Thinking"
    /// label instead of being hidden entirely by the Complete-state fast
    /// path in the faithful renderer).  Messages-list uses this to decide
    /// whether the row contributes vertical space when unselected.
    bool thinking_active = false;
    bool is_local_command_input = false;
    bool is_local_command_output = false;
    bool is_local_jsx_output = false;
    bool is_compact_boundary = false, is_error = false;
    /// True when the entry corresponds to a user-attached ImageBlock and
    /// should be routed through MessageShape::UserImage (instead of
    /// UserText).  Populated by project_messages when splitting a single
    /// UserMessage with mixed text+image content into multiple sibling
    /// display rows (each <UserImageMessage/> is its own row).
    bool is_image = false;
    std::optional<::loom::core::ImageBlock> image_block;
    /// Display ID for user-attached images (shown as "[Image #N]").
    /// Populated by project_messages when splitting a UserMessage with
    /// image content blocks into individual display rows
    /// (imageIds assigned from message.imagePasteIds).
    std::optional<int> image_display_id;
    std::optional<std::string> tool_name, tool_status;
    /// Parsed tool input JSON for tool-use entries.  Threaded into
    /// ToolUseRenderOptions.raw_parameters (shared G1/G2 contract) instead of
    /// the previous content_preview fallback.  G2 (app.cppm) populates this
    /// from ToolUseBlock.input_json / ToolUseMessage.tool_input_json.
    std::optional<std::string> tool_input_json;
    /// M6 result_preview: live streaming result preview for tool-use entries.
    /// Populated by App::UpdateScreen from streaming tool state, consumed by
    /// BuildMessagesList to drive ToolUIRegistry.progress() and the
    /// result-preview block in faithful tool-use renderers.
    std::optional<std::string> tool_result_preview;
    /// Structured content items from tool results
    /// (e.g. MCP tools returning mixed text+image).  When present, the
    /// faithful tool-result renderer iterates these instead of the
    /// flattened content_preview string.
    std::optional<std::vector<::loom::core::ToolResultContentItem>> tool_result_content_items;
    std::optional<std::string> agent_display_name, agent_color_name;
    std::chrono::system_clock::time_point timestamp;
    /// Thinking duration in milliseconds (populated for thinking entries).
    /// Computed from streaming start/end times; carried over to committed
    /// entries so the collapsed "∴ Thought for Xs" label survives the
    /// streaming→committed transition.
    std::chrono::milliseconds thinking_duration{0};
    int estimated_height_lines = 3;
    /// M4 live-path: system-row subtype hint.  When set on a `system` entry,
    /// RenderMessages routes the row through the matching faithful
    /// RenderSystemTextMessageFaithful branch (away_summary / teardrop event /
    /// generic dot).  When unset the projection falls back to a heuristic
    /// derived from content_preview (see RenderMessages).  The engine / G2
    /// app.cppm may populate this from the upstream SystemMessage tag later.
    std::optional<std::string> system_subtype;

    // ── P2 gap api-error-retry: retry metadata for SystemAPIError cards ──
    /// retryInMs.  Backoff duration in
    /// milliseconds before the next auto-retry.  Used for the live countdown.
    std::optional<double> retry_after_ms;
    /// retryAttempt.  Current attempt
    /// number (1-based, shown as "attempt N/M").
    std::optional<int> retry_attempt;
    /// maxRetries.  Total allowed attempts.
    std::optional<int> max_retries;
    /// sessionExpired.  When true, the
    /// auth session has expired; show "Clear session" button instead of Retry.
    bool session_expired{false};
};

/// RFC 0002 F3 store — message-list, scroll and transcript-chrome state,
/// sharded out of ReplScreenState. Homed in loom.ui.screens (rank 10): the
/// concrete cross-area field types live in areas ranked below screens, so
/// by-value fields recreate no up-edge. UI-thread-affined plain data —
/// see the file header for the threading and import rules.
struct MessagesStore {
    /// Minimal Message projection rows for orchestration.
    /// Per-row rendering delegated to message_row.cppm (UI4/UI5).
    std::vector<MessageDisplayEntry> messages;

    /// Scratch rows used only while a local command overlay is active.
    mutable std::vector<MessageDisplayEntry> local_overlay_rows;

    /// Retained view payloads. Only changed source rows are projected again.
    ::loom::ui::messages_list::MessagesListInput render_input;
    std::vector<MessageDisplayEntry> render_sources;
    bool render_sources_transcript_mode = false;

    // ── Scroll / transcript chrome ──────────────────────────────────────
    int scroll_offset = 0, selected_message_idx = -1;
    int viewport_height_lines = 40;
    bool scroll_pinned_to_bottom = true;

    /// Index into messages[] where the unseen divider anchor sits.
    /// Set on FIRST scroll-away from bottom (the unseen-divider
    /// anchor).  nullopt = pinned to bottom (no divider).  Cleared
    /// on repin (scroll-to-bottom, submit, or /clear).
    std::optional<std::size_t> divider_index;
    /// Snapshot of messages.size() at the time of first scroll-away.
    /// Used to detect when new messages arrive past the divider.
    std::size_t message_count_at_scroll_away = 0;

    // P0-3 VirtualMessageList — the imperative handle exposed by the windowed
    // renderer when the current visible count crosses kVirtualThreshold.
    // `virtual_list_active` is set per-frame by RenderMessages; when true,
    // ScrollTranscript uses JumpToVisualLine() which runs O(log N) binary
    // search against the JumpHandle instead of the crude EstimateTranscriptRows.
    bool virtual_list_active = false;
    loom::ui::messages::virtual_list::JumpHandle virtual_jh;
    std::shared_ptr<loom::ui::messages::virtual_list::VirtualListState>
        virtual_list_state;

    // M1 (FullscreenLayout slot-system chrome): scroll-derived chrome state.
    //   sticky_prompt — std::nullopt = at bottom (null).  Value present =
    //     scrolled up; the inner .text is shown as a breadcrumb header and
    //     .scroll_target_row is the absolute transcript row to jump to on
    //     click.
    //   sticky_prompt_clicked — set to true by the header's click handler
    //     BEFORE the actual scroll happens; hides the header this
    //     frame so the prompt line rises to absolute row 0 of the scroll
    //     viewport (the collapsed-padding mechanic already strips the top
    //     padding whenever sticky_prompt is set, so the only delta is hiding
    //     the header row).  Reset to false by any subsequent scroll event that
    //     writes a fresh sticky_prompt (the tracker emits every
    //     viewport-top change while unpinned).
    //   unseen_message_count — count of assistant turns added below the fold
    //     while unpinned; drives the "N new messages" pill label.
    //     The pill itself only
    //     renders when pill_visible is true (engine snapshots the divider).
    //   pill_visible — whether the pill is shown (subscribed against the
    //     scroll handle).  Defaults false so chrome stays dormant until the
    //     engine wires real scroll-observe state.
    // 3-state discriminant + collapsed-padding logic.
    std::optional<::loom::ui::layout::fullscreen::StickyPrompt> sticky_prompt;
    bool sticky_prompt_clicked = false;
    int unseen_message_count = 0;
    bool pill_visible = false;

    // unseenDivider prop + UnseenDivider + computeUnseenDivider.
    // Populated by App::UpdateScreen from scroll state + messages[].  The
    // engine sets `first_unseen_uuid_prefix` to the 24-char prefix (or full
    // uuid) of messages[dividerIndex] after skipping progress + null-rendering
    // attachments (CC-724).  `count` is Math.max(1, countUnseenAssistantTurns(…)).
    // nullopt = no divider (pinned to bottom, dividerIndex=null, empty session).
    std::optional<::loom::ui::messages_list::UnseenDivider> unseen_divider;

    /// Mouse hit-testing tracker for click-to-expand rows (thinking blocks).
    /// The renderer (RenderMessages → render_messages_list_view) populates
    /// this with screen-space boxes each frame; the event handler
    /// (repl_screen_events.cpp) uses hit_test() to map a click → row index.
    ::loom::ui::messages_list::RowClickTracker row_click_tracker;
};

/// Reset scroll to the bottom (pinned).  Replaces the 4-line
/// scroll_offset=0 + scroll_pinned_to_bottom=true block that was
/// previously copy-pasted at 4+ call sites.
inline void ResetScrollToBottom(MessagesStore& store) {
    store.scroll_offset = 0;
    store.scroll_pinned_to_bottom = true;
}

}  // namespace loom::ui
