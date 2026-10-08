/// =========================================================================
/// @file messages_list.cppm
/// @brief Messages list container + single-message envelope wrapper.
///
/// UI21 RESPONSIBILITY — "List container + single-message envelope" (REPL main path P0 blocker)
///   Responsibilities:
///     - build_visible_rows : O(N) filter + search + compact-group collapsing
///     - selection / streaming-tail cursor / last-N render cap
///     - keyboard nav (j/k, g/G, / search, c/r/d actions, Space toggle)
///     - yframe scroll + vscroll_indicator, empty-state rendering
///     - render_message_envelope: avatar column + role pill + timestamp
///       + status badge + dismiss + per-role accent border (Top)
///     - variant dispatch to RenderMessageRowByType (UI4 message_row)
///
/// DEPENDENCIES (STRICT — no type/color duplication):
///   import loom.ui.messages.message_row;
///       -> MessageShape enum  +  MessageRowPayload variant
///       +  RenderMessageRowByType(shape, payload, callbacks)
///   import loom.ui.messages.virtual_list;  // virtual-row conversion types
///   import loom.ui.visual.markdown;        // StreamingMarkdown member type
///
///   Bodies live in the seven module implementation units (RFC 0001 Phase C
///   batch 7): messages_list_{filter,search,geometry,envelope,payload_row,
///   view,component}.cpp.  The per-message-type variant owner modules and the
///   tool registry are imported ONLY by messages_list_search.cpp and
///   messages_list_payload_row.cpp, so the ~20-module variant closure stays
///   out of this interface's BMI.
///
///   Palette lookups go through the small inline helpers `palette::*()`
///   (raw ftxui::Color) so that swapping in real design tokens is a
///   one-line grep — nothing else changes.
/// =========================================================================
///
/// ┌───────────────────────────────────────────────────────────────────────┐
/// │  PUBLIC API                                                           │
/// ├───────────────────────────────────────────────────────────────────────┤
/// │  struct MessagesListInput                                             │
/// │    ├─ vector<MessageRowPayload> rows                                  │
/// │    ├─ vector<MessageShape>       shapes   (parallel to rows)         │
/// │    ├─ optional<size_t> selected_row_idx                               │
/// │    ├─ size_t streaming_tail_row   (= rows.size() when not streaming) │
/// │    ├─ vector<pair<size_t,size_t>> compact_boundary_groups            │
/// │    ├─ Filters  {show_system, show_tool_in, show_tool_out,            │
/// │    │             show_thinking, show_compact}                         │
/// │    ├─ string search_query    (case-insensitive substring match)      │
/// │    └─ optional<size_t> jump_to_row_on_init                           │
/// │                                                                      │
/// │  enum class ActionKind { Copy, Regenerate, Delete }                  │
/// │                                                                      │
/// │  struct MessagesListCallbacks                                        │
/// │    ├─ on_select(size_t row_idx)                                      │
/// │    ├─ on_action(size_t row_idx, ActionKind)                          │
/// │    ├─ on_toggle_compact_group(size_t group_idx)                      │
/// │    ├─ on_click_attachment(size_t row_idx, size_t attachment_idx)     │
/// │    └─ on_search_changed(const string& query)                         │
/// │                                                                      │
/// │  build_visible_rows(input) -> vector<VisibleRow>   (pure O(N))       │
/// │                                                                      │
/// │  render_messages_list_view(input, frame_count) -> Element            │
/// │    ├─ STATIC / NON-INTERACTIVE variant                               │
/// │    ├─ used by dialog builders that just want the visual list        │
/// │    └─ frame_count drives blink + spinner glyph                       │
/// │                                                                      │
/// │  MakeMessagesList(input, callbacks) -> Component                     │
/// │    ├─ FULLY INTERACTIVE variant                                      │
/// │    ├─ keyboard nav + embedded search Input + action hotkeys          │
/// │    └─ delegates per-row rendering to envelope + UI4 dispatch        │
/// │                                                                      │
/// │  render_message_envelope(...) -> Element                             │
/// │    ├─ 36-col avatar column + role pill + timestamp + status badge   │
/// │    ├─ per-role top accent border                                     │
/// │    └─ inner = RenderMessageRowByType payload dispatch                │
/// └───────────────────────────────────────────────────────────────────────┘
///
/// CAPS:
///   * MAX_ROWS_RENDERED_LAST_N = 80
///       -> non-virtualized "last-N" window for render.  The UI22
///          VirtualScroll wrapper is expected to bump this to 200+.
///   * build_visible_rows is O(N) over rows.size() (<= 100 000) and
///     guaranteed sub-16 ms on reasonable hardware (single pass, no
///     allocations per-row beyond string::find on lowered text).
/// =========================================================================

module;

#include <cstdint>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>

export module loom.ui.messages.messages_list;

import std;

// ─── Strict re-uses (no type / colour duplication) ──────────────────────
import loom.ui.messages.message_row;
import loom.ui.messages.virtual_list;   // P0-3: VirtualMessageList types + factory
// StreamingMarkdown is named by the MessagesListInput::streaming_md member.
// Per-message-type variant owner modules (user_text_message, tool_use_message,
// …) are imported ONLY by the module implementation units that name their
// alternatives — messages_list_search.cpp (the std::visit closure) and
// messages_list_payload_row.cpp (faithful dispatch) — keeping them out of
// this interface's BMI.
import loom.ui.visual.markdown;   // arch-check: keep-import — StreamingMarkdown* member (global-qualified; checker sees only unqualified uses)
// =========================================================================
// Small palette helpers — tokens placeholders (swap for loom.ui.foundation.design_tokens)
// =========================================================================
// Each lookup returns an ftxui::Color.  Kept in a single namespace so the
// grep-replace for real tokens is mechanical.

namespace loom::ui::messages_list::palette {

using ftxui::Color;

inline auto role_bg_user()        -> Color { return Color::RGB(30, 41, 59); } // TODO(token): message_role_user_background
inline auto role_bg_assistant()   -> Color { return Color::RGB(30, 41, 59); } // TODO(token): message_role_assistant_background
inline auto role_bg_system()      -> Color { return Color::RGB(30, 30, 36); } // TODO(token): message_role_system_background
inline auto role_bg_tool()        -> Color { return Color::RGB(24, 40, 40); } // TODO(token): message_role_tool_background
inline auto role_bg_thinking()    -> Color { return Color::RGB(34, 30, 48); } // TODO(token): message_role_thinking_background

inline auto role_pill_user()      -> Color { return Color::RGB(59, 130, 246); } // TODO(token): message_role_user_pill
inline auto role_pill_assistant() -> Color { return Color::RGB(168, 85, 247); } // TODO(token): message_role_assistant_pill
inline auto role_pill_system()    -> Color { return Color::RGB(234, 179,  8); } // TODO(token): message_role_system_pill
inline auto role_pill_tool()      -> Color { return Color::RGB(20, 184, 166); } // TODO(token): message_role_tool_pill
inline auto role_pill_thinking()  -> Color { return Color::RGB(139, 92, 246); } // TODO(token): message_role_thinking_pill

inline auto accent_top_user()     -> Color { return role_pill_user(); }
inline auto accent_top_assistant()-> Color { return role_pill_assistant(); }
inline auto accent_top_system()   -> Color { return role_pill_system(); }
inline auto accent_top_tool()     -> Color { return role_pill_tool(); }
inline auto accent_top_error()    -> Color { return Color::RGB(239, 68, 68); } // TODO(token): message_error_accent
inline auto accent_top_redacted() -> Color { return Color::RGB(107, 114, 128); } // TODO(token): message_redacted_accent

inline auto selected_bg()         -> Color { return Color::RGB(30, 64, 175); } // TODO(token): message_list_selected_background
inline auto muted_fg()            -> Color { return Color::RGB(156, 163, 175); } // TODO(token): message_list_muted_foreground
inline auto empty_state_fg()      -> Color { return Color::RGB(107, 114, 128); } // TODO(token): message_list_empty_state_foreground
inline auto streaming_fg()        -> Color { return Color::RGB(34, 211, 238); } // TODO(token): message_list_streaming_foreground

} // namespace loom::ui::messages_list::palette

// =========================================================================
export namespace loom::ui::messages_list {

using namespace ftxui;
using namespace messages;  // UI4: MessageShape, MessageRowPayload, …

using ::loom::ui::messages::MessageShape;
using ::loom::ui::messages::MessageRowPayload;
using ::loom::ui::messages::MessageRowCallbacks;
using ::loom::ui::messages::RenderMessageRowByType;

// =========================================================================
// 1)  Input types
// =========================================================================

struct Filters {
    bool show_system   = true;
    bool show_tool_in  = true;
    bool show_tool_out = true;
    bool show_thinking = true;
    bool show_compact  = true;   // if false, collapsed groups render as rows
};

// ---------------------------------------------------------------------------
// Brief-mode filter
//
// When is_brief_mode is true, only show:
//   1. System messages (except api_metrics subtype — not available in CPP so
//      we keep all system rows)
//   2. Assistant tool_use rows whose tool_name is in the brief-tool set
//      (Brief, SendUserMessage)
//   3. User tool_result rows (paired with brief tool uses above)
//   4. Real user text input (not meta/tick messages — not tagged in CPP so
//      we keep all UserText/UserPrompt rows)
//
// Hidden: assistant text, non-brief tool uses, thinking blocks, attachments.
// ---------------------------------------------------------------------------
namespace brief_detail {

/// Tool names that constitute the "brief" tool chain.
inline constexpr std::string_view kBriefToolNames[] = {
    "Brief", "SendUserMessage", "SendUserFile"
};

/// For dropTextInBriefTurns (default mode, not brief-only), only turns that
/// called Brief or SendUserMessage should have their assistant text dropped.
/// SendUserFile delivers a file without replacement text, so dropping text
/// for file-only turns would leave the user with no context.
inline constexpr std::string_view kDropTextToolNames[] = {
    "Brief", "SendUserMessage"
};

[[nodiscard]] bool is_brief_tool_name(std::string_view name);

/// Returns true if the tool name triggers dropTextInBriefTurns.
[[nodiscard]] bool is_drop_text_tool_name(std::string_view name);

/// Extract tool_name from a MessageRowPayload if it is a tool_use or
/// tool_result variant.  Returns empty string otherwise.
[[nodiscard]] std::string_view extract_tool_name(const MessageRowPayload& p);

}  // namespace brief_detail

/// Returns true if the row at index `i` should be VISIBLE in brief mode.
auto passes_brief_filter(
    MessageShape shape,
    const MessageRowPayload& payload) -> bool;

// ---------------------------------------------------------------------------
// dropTextInBriefTurns.
//
// In default mode (neither transcript nor brief-only), drops assistant TEXT
// rows in turns that called a Brief/SendUserMessage/SendUserFile tool.  The
// model's text output is redundant with the SendUserMessage content it wrote
// right after — dropping it keeps the transcript focused on tool output.
//
// Per-turn: only drops text in turns that actually called a Brief tool.  If
// the model forgets to call Brief, text still shows — otherwise the user
// would see nothing for that turn.
//
// Returns a vector<bool> mask (true = KEEP the row, false = DROP it).
// ---------------------------------------------------------------------------
[[nodiscard]] std::vector<bool> compute_drop_text_mask(
    const std::vector<MessageShape>& shapes,
    const std::vector<MessageRowPayload>& payloads);

// ---------------------------------------------------------------------------
// Expand-key computation
//
// For tool_use and tool_result rows, returns the tool_name so a tool_use
// and its corresponding tool_result share the same key and expand together.
// For other rows, returns the uuid (or empty string if not available).
// ---------------------------------------------------------------------------
[[nodiscard]] std::string compute_expand_key(
    MessageShape shape,
    const MessageRowPayload& payload,
    std::string_view uuid);

/// Returns true if the row supports click-to-expand: tool results that are
/// truncated, collapsed read/search groups, or advisor tool results.
[[nodiscard]] bool is_row_clickable(
    MessageShape shape,
    const MessageRowPayload& payload);

/// In-transcript "N new messages" divider anchor + count.  When set, the
/// renderer inserts a muted separator line BEFORE the first renderable row
/// whose uuid shares the 24-char prefix with firstUnseenUuid.
struct UnseenDivider {
    /// UUID (or 24-char prefix) of the first message that arrived after the
    /// user scrolled away from the tail.  The render side matches on the
    /// first 24 chars (the prefix is preserved across derived content blocks
    /// so grouped rows still match).
    std::string first_unseen_uuid_prefix;
    /// Number of new assistant turns (floors at 1 if any unseen content
    /// exists).
    std::size_t count = 0;
};

// ---------------------------------------------------------------------------
// Row-click tracking (mouse hit-testing for click-to-expand)
//
// The pure-function render path (render_messages_list_view) doesn't have a
// Component to hold state, so callers that want mouse click-to-expand pass
// a RowClickTracker pointer through MessagesListInput.  The renderer calls
// begin_frame() + track_row() during Render() (attaching reflect() boxes to
// clickable rows), and the event handler calls hit_test() to map a mouse
// click to an expand key that can be toggled in expanded_keys.
//
// Boxes are reused across frames (same pattern as MessagesListComponent's
// tracked_boxes_) because the previous frame's element tree still holds
// Box& references via reflect() — destroying them before the old tree is
// replaced would be use-after-free.
// ---------------------------------------------------------------------------
struct RowClickTracker {
    std::vector<std::unique_ptr<Box>> boxes;
    std::vector<std::string>          expand_keys;  // parallel to boxes
    std::size_t                       cursor = 0;

    /// Reset the per-frame cursor.  Call once at the start of each render.
    void begin_frame() noexcept;

    /// Record a clickable row by its expand key and return a Box& for the
    /// renderer to attach via reflect().  Reuses existing boxes across
    /// frames.
    [[nodiscard]] auto track_row(std::string expand_key) -> Box&;

    /// Trim excess boxes from previous frames.  Call at the end of render.
    void end_frame() noexcept;

    /// Return the expand key at screen position (x, y), or nullopt.
    [[nodiscard]] auto hit_test(int x, int y) const noexcept
        -> std::optional<std::string>;
};

struct MessagesListInput {
    std::vector<MessageRowPayload> rows;
    /// Parallel to `rows`.  The dispatcher needs both shape and payload.
    std::vector<MessageShape>       shapes;
    /// Parallel to `rows`.  Used by the UnseenDivider prefix-match anchor
    /// (24-char prefix match).  Empty strings are allowed —
    /// rows without a uuid simply never match the divider anchor.
    std::vector<std::string>        uuids;

    std::optional<std::size_t>      selected_row_idx;
    /// Row at which the spinner + blinking "generating…" cursor is drawn.
    /// When == rows.size() the tail is NOT rendered (not streaming).
    std::size_t                     streaming_tail_row = 0;

    /// Compact groups as half-open [start, end) intervals over `rows`.
    /// When show_compact=true each group appears as a SINGLE collapsed row
    /// ("📦 N messages collapsed …"); when false the inner rows render.
    std::vector<std::pair<std::size_t, std::size_t>> compact_boundary_groups;

    Filters                         filters;
    std::string                     search_query;   // case-insensitive
    std::optional<std::size_t>      jump_to_row_on_init;
    bool                            pin_to_bottom = false;
    int                             scroll_offset = 0;
    int                             viewport_rows = 40;

    /// When true, only brief-tool calls + their results + real user input
    /// are shown; assistant text, thinking, and non-brief tools are hidden.
    bool                            is_brief_mode = false;

    /// When true, bypass brief/dropText filters and show ALL message types.
    /// Capped at last 30 messages unless show_all_in_transcript is also true.
    bool                            is_transcript_mode = false;

    /// When true AND is_transcript_mode, lifts the 30-message cap so ALL
    /// messages are rendered.  Toggled by user (Ctrl+E in transcript mode).
    bool                            show_all_in_transcript = false;

    /// When true, skips chain compression (Step 2.5) entirely.  Used by
    /// tests that need to verify individual tool/thinking row rendering.
    /// Production code always leaves this false.
    bool                            disable_chain_compression = false;

    /// True while a query is actively streaming.  When set, the last
    /// compressible chain renders as a LIVE status row (spinner + elapsed
    /// timer + tool activity) instead of a static summary.  Synced from
    /// AppAdapter::query_running_ via ReplScreenState.
    bool                            query_running = false;

    /// Optional pointer to a shared StreamingMarkdown instance used for the
    /// streaming-text tail row.  When set and the row's is_streaming=true,
    /// render_payload_row passes this to RenderAssistantTextMessageFaithful
    /// so the body uses stable-prefix caching instead of full re-parse.
    /// Nullptr = not streaming / use plain render_markdown.
    ::loom::ui::StreamingMarkdown*    streaming_md = nullptr;

    /// Optional pointer to a RowClickTracker that the renderer populates
    /// with screen-space boxes of clickable rows (thinking blocks, etc.).
    /// The event handler uses it to map mouse clicks → row indices for
    /// click-to-expand.  Nullptr = no mouse tracking (golden tests, dialogs).
    RowClickTracker*                row_click_tracker = nullptr;

    /// Set of "expand keys" that the user has clicked/pressed-Enter on to
    /// reveal full content.  Keys are tool_name strings for tool_use/tool_result
    /// rows (so a tool_use and its tool_result expand together), or uuid
    /// prefixes for other row types.  Empty = nothing expanded.
    std::unordered_set<std::string> expanded_keys;

    /// When set, a colored divider line is inserted before the matching row.
    std::optional<UnseenDivider>    unseen_divider;

    /// Per-row cache of lowered searchable text.  Indexed by row_idx (parallel
    /// to rows[]).  `mutable` so the cache can be populated through const-ref
    /// accessors (visible_rows_to_virtual takes `const MessagesListInput&`).
    ///
    /// Cache invalidation: messages are append-only and immutable.  In C++
    /// the cache vector grows to match rows.size() on first access; entries
    /// are never invalidated because row content never changes after
    /// projection.
    mutable std::vector<std::optional<std::string>> lowered_search_cache;

    /// GAP 3: msg-system-api-error-retry — callback for the "Retry" button
    /// on SystemAPIError rows.  When set, the API error card renders a
    /// clickable Retry pill that invokes this to re-send the last user
    /// message.
    std::function<void()> on_retry;
    /// P2 gap api-error-retry: callback for "Clear session" button on
    /// session-expired SystemAPIError rows.
    std::function<void()> on_clear_session;

    /// Output: when the virtual (windowed) render path is used, the renderer
    /// populates this with the JumpHandle (exact prefix-sum geometry) so the
    /// caller can set MessagesStore::virtual_jh for precise scroll bounds.
    /// Mutable because render_messages_list_view takes a const-ref input.
    mutable std::optional<virtual_list::JumpHandle> virtual_jh_out;
};

enum class ActionKind { Copy, Regenerate, Delete };

struct MessagesListCallbacks {
    std::function<void(std::size_t)>                       on_select;
    std::function<void(std::size_t, ActionKind)>           on_action;
    std::function<void(std::size_t)>                       on_toggle_compact_group;
    std::function<void(std::size_t, std::size_t)>          on_click_attachment;
    std::function<void(const std::string&)>                on_search_changed;
    /// Called when the user presses Enter/Space on a clickable row to toggle
    /// its expanded state.  The key is the expandKey (tool_name for tool rows,
    /// uuid for others) so tool_use + tool_result expand together.
    std::function<void(const std::string& expand_key)>     on_toggle_expand;
};
/// Returns true if the row at index `i` is currently "expanded" (user has
/// toggled it open via Enter/Space).  Expanded rows render with verbose=true
/// showing full content instead of truncated summaries.
[[nodiscard]] bool is_row_expanded(
    const MessagesListInput& input,
    std::size_t row_idx);

// =========================================================================
// 2)  Preview-text extractor (works on every MessageRowPayload alternative)
// =========================================================================
// Used by build_visible_rows for the case-insensitive substring filter.
// Falls back to std::visit over every variant branch so we never need a
// virtual `preview_text()` method.

namespace detail {

/// Lower-case a UTF-8 string in place.  Only touches ASCII letters because
/// MessageRowPayload text fields are overwhelmingly English / path literals
/// (same strategy as renderableSearchText → toLowerCase).
auto lowered(std::string s) -> std::string;

auto payload_preview(const MessageRowPayload& p) -> std::string;

// =========================================================================
// 2b)  extract_search_text — RICH searchable text for indexing (Tier 2)
// =========================================================================
//
// Two-tier search text extraction:
//   Tier 1 (toy):    payload_preview() — short display-friendly summaries
//                    used for compact-group labels, quick previews.
//   Tier 2 (rich):   extract_search_text() — detailed text from tool results
//                    (file contents, bash output, grep matches) used for
//                    search indexing.  Falls back to payload_preview() for
//                    non-tool message types.
//
// For tool-result messages, the per-tool registry lookup provides precise
// tool-owned extraction (matching what render_tool_result_message shows).
// Tools that don't show content on screen (FileRead, FileWrite, WebSearch)
// return "" to avoid phantom matches.

namespace search_detail {

/// Extract a string field value from a JSON object string.
/// Lightweight — no full JSON parser needed for known field names.
[[nodiscard]] std::string extract_json_field(
    std::string_view json, std::string_view field_name);

/// Extract searchable text from a tool-use input JSON string.
/// Extracts known field names that render_tool_use_message
/// shows as the primary argument (command, pattern, file_path, etc.).
[[nodiscard]] std::string tool_use_search_text(std::string_view input_json);

} // namespace search_detail

/// Rich searchable text for indexing.  Returns detailed content from tool
/// results (file contents, bash output, grep matches) for search matching.
/// Falls back to payload_preview() for non-tool message types.
///
/// @param p       The message row payload variant.
/// @param shape   The message shape (for dispatch optimization).
/// @return Lowercase-rich searchable text (NOT lowered — caller lowers).
[[nodiscard]] auto extract_search_text(
    const MessageRowPayload& p,
    MessageShape shape) -> std::string;

// =========================================================================
// 2c)  Cached lowered search text accessor
// =========================================================================
//
// Returns the LOWERED rich searchable text for row_idx, using the per-row
// cache on MessagesListInput.  First call computes + caches; subsequent
// calls (build_visible_rows then visible_rows_to_virtual) hit the cache
// with zero alloc.
//
// Cache is keyed by row_idx (parallel to input.rows) — messages are
// append-only and immutable, so a cached entry is always valid.
[[nodiscard]] auto get_cached_lowered_search_text(
    const MessagesListInput& input,
    std::size_t row_idx,
    MessageShape shape) -> std::string;

/// Returns true for message SHAPEs that belong to each filter category
/// (system / tool_use / tool_result / thinking / compacted).
auto shape_category(MessageShape s) -> std::string_view;

auto passes_filters(MessageShape s, const Filters& f) -> bool;

} // namespace detail

// =========================================================================
// 3)  Visible-row representation
// =========================================================================

/// When is_transcript_mode=true and show_all_in_transcript=false, only the
/// last this-many visible rows are rendered.  A divider row shows how many
/// older messages were hidden.
constexpr std::size_t kMaxMessagesInTranscriptMode = 30;

/// A single row that render_messages_list_view / the Component will emit.
/// Either a real payload row, a compact-group synthetic row, a compressed-
/// chain synthetic row (thinking + tool calls collapsed to one summary
/// line), or a transcript-cap divider ("N older messages hidden — Ctrl+E
/// to show all").
struct VisibleRow {
    enum class Kind { Payload, CompactGroup, TranscriptCapDivider, CompressedChain };

    Kind kind = Kind::Payload;

    // For Kind::Payload — index into MessagesListInput.rows/shapes
    std::size_t row_idx = 0;

    // For Kind::CompactGroup — index into compact_boundary_groups, plus stats
    std::size_t group_idx       = 0;
    std::size_t group_count     = 0;   // number of original rows in group
    std::size_t tool_turns      = 0;   // tool calls in group/chain
    std::size_t additions       = 0;
    std::size_t deletions       = 0;

    // For Kind::TranscriptCapDivider — number of messages hidden by the cap.
    // For Kind::CompressedChain — group_count = thinking blocks in chain,
    // tool_turns = tool calls in chain, chain_tool_breakdown = per-tool-type
    // counts, chain_thinking_duration = total thinking time, chain_is_live =
    // true while the query is still streaming (renders as a live status row).
    std::size_t hidden_count    = 0;

    // For Kind::CompressedChain — rich summary data.
    bool                            chain_is_live = false;
    std::chrono::milliseconds       chain_thinking_duration{0};
    std::vector<std::pair<std::string, std::size_t>> chain_tool_breakdown = {};
    /// UUID of the chain's anchor row (first row in the chain).
    /// Used as the expand/collapse key: "chain:" + chain_uuid.
    std::string                     chain_uuid = {};
    /// True when this chain header represents an expanded chain (the
    /// individual rows follow as payload).  Clicking the header collapses.
    bool                            chain_is_expanded = false;
};
// =========================================================================
// 4)  build_visible_rows  —  pure O(N) over input.rows
// =========================================================================
/// Step 1 : per-row filter + search (Filters + search_query)
/// Step 2 : if show_compact, collapse compact_boundary_groups into a
///          single "CompactGroup" visible row each.
///
/// Complexity guarantee: exactly ONE linear pass over input.rows plus ONE
/// pass over compact_boundary_groups (sorted).  Uses a std::vector<bool>
/// membership table for O(1) "is this row inside a compact group?" lookups.
auto build_visible_rows(MessagesListInput& input) -> std::vector<VisibleRow>;

// =========================================================================
// 3b) Estimated row height + VisibleRow → virtual_list::VisibleRow converter
// =========================================================================
//
// P0-3 virtual scroll needs per-row LINE estimates so JumpHandle's prefix-
// sum table produces accurate scroll thumbs.  These estimates are HEURISTIC:
//   • Short assistant text / user prompt   →  2-3 lines (min 1)
//   • Long content (tool result, bash)     →  4 + wrapped lines (capped 80)
//   • Thinking (compact)                   →  1 line  (hidden when complete)
//   • Compact group row                    →  1 line
//
// The default per-message estimate is 3 lines; our VisibleRow
// is *finer-grained* so we target mean ≈ 2.2 lines / VisibleRow.

namespace detail {

/// Count *wrapped* lines for `text` given terminal columns.  Uses a
/// text-wrap heuristic (hard-break at term_cols, plus existing '\n').  The
/// result is the maximum vertical space the content COULD take inside a
/// 36-col reserved left-gutter message envelope; 36 is subtracted from
/// term_cols to account for the fixed avatar column.
[[nodiscard]] auto estimate_content_lines(
    std::string_view text,
    int term_cols,
    int envelope_gutter_cols = 36) -> int;

/// Estimate visual height for one messages_list::VisibleRow.  Adds 1 line
/// for the envelope's top-accent + role-header row and 1 for trailing
/// separator (except for 1-line rows where it collapses).
[[nodiscard]] auto estimate_row_height(
    const VisibleRow& vr,
    const MessagesListInput& input,
    int term_cols) -> int;

} // namespace detail

/// Convert a messages_list `VisibleRow` vector into the format consumed by
/// VirtualMessageList.  Each entry carries:
///   - row_id           stable hash (for future cache invalidation)
///   - estimated_hl     content-aware estimate (lines)
///   - search_key       lowered preview text (for scroll_search callback)
///   - type_hint        0 = Payload, 1 = CompactGroup
///   - backend_index    round-trip key for render_row callback
[[nodiscard]] auto visible_rows_to_virtual(
    const std::vector<VisibleRow>& visible,
    const MessagesListInput& input,
    int term_cols = 80)
    -> std::vector<loom::ui::messages::virtual_list::VisibleRow>;

/// Decode the backend_index set by `visible_rows_to_virtual` back into a
/// messages_list::VisibleRow.  `out` is filled in place; returns true if
/// decode succeeded, false if the key was malformed (out is reset to a
/// safe payload(0) sentinel on failure).
[[nodiscard]] bool decode_virtual_backend_index(
    std::uint64_t backend_index,
    VisibleRow& out) noexcept;

// =========================================================================
// 3c) Render path: P0-3 VirtualMessageList when visible_rows > threshold
// =========================================================================
//
// Threshold kVirtualThreshold (default 80) matches the legacy Last-N cap
// AND VirtualMessageList.  Below threshold we still render everything
// via render_messages_list_view (simple, deterministic, no overscan).
// At/above threshold, this function:
//   (1) converts visible_rows → virtual rows (with height estimates)
//   (2) calls render_list_as_elements (spacer+slice+spacer, O(visible) not O(N))
//   (3) forwards render_row back to messages_list's per-row envelope renderer
//
// Returns a fully-rendered FTXUI Element (yframe + vscroll_indicator applied).
// Accepts an optional `scroll_top_lines` value so ReplScreen's existing
// scroll_offset plumbing can still drive the initial window position.

inline constexpr std::size_t kVirtualThreshold = 80;

[[nodiscard]] auto render_messages_list_virtual(
    const MessagesListInput& input_const,
    std::size_t frame_count,
    int viewport_rows,
    int scroll_top_lines) -> Element;

// =========================================================================
// 5)  Message envelope  (role chrome + avatar)
// =========================================================================
/// The outer per-message chrome that wraps
/// RenderMessageRowByType's inner output.  Layout:
///
///   ┌─────────────────────────────────────────────────────────┐
///   │ ▲ accent_top (1-px, role-colored)                       │
///   ├────────────┬────────────────────────────────────────────┤
///   │            │  [Assistant ●●●]  [⏱ 14:32]  [running] ✕  │  header
///   │ AVATAR     ├────────────────────────────────────────────┤
///   │ (36 cols)  │  inner = RenderMessageRowByType(...)      │  body
///   │            │                                            │
///   └────────────┴────────────────────────────────────────────┘
///
/// Status badge values:
///   "running" → cyan spinner dot
///   "error"   → red pill
///   "done"    → green check
///   "redacted"→ dim grey pill
///   ""        → hidden

enum class EnvelopeStatusBadge { None, Running, Error, Done, Redacted };

struct RenderEnvelopeOptions {
    MessageShape shape;
    std::chrono::system_clock::time_point timestamp =
        std::chrono::system_clock::now();
    EnvelopeStatusBadge status = EnvelopeStatusBadge::None;
    bool show_dismiss = false;
    bool is_selected  = false;
    std::size_t frame_count = 0;          // for spinner glyph
};

namespace detail {

auto role_emoji(MessageShape s) -> const char*;
auto role_label(MessageShape s) -> const char*;
auto role_pill_color(MessageShape s) -> Color;
auto role_bg_color(MessageShape s) -> Color;
auto accent_top_color(const RenderEnvelopeOptions& o) -> Color;
auto spinner_glyph(std::size_t frame) -> const char*;

} // namespace detail

[[nodiscard]] auto render_message_envelope(
    const RenderEnvelopeOptions& opts,
    Element inner_content) -> Element;

// =========================================================================
// 6)  Per-visible-row element builders
// =========================================================================

namespace detail {

// ─── UnseenDivider helpers ────────────────────────────────────────────────

/// Return the 24-char prefix of s (or whole s if shorter).  The prefix
/// preserves the source message uuid's first 24 chars across derived content
/// blocks, so matching on prefix captures every renderable row that came
/// from the same original unseen message.
[[nodiscard]] auto uuid_prefix24(std::string_view s) -> std::string_view;

/// Two-tier search (tolerates synthetic uuid padding):
///
///   WEAK (baseline):  first VisibleRow whose payload-row uuid matches
///       the divider anchor on the first 24 chars.
///
///   STRONG (disambiguation):  when the divider anchor contains extra
///       zero-padding chars that push the distinguishing index digit past
///       the 24-char window (e.g. target = old0000000000000000000003, 25 chars
///       where the intended uuid row is only 24 chars), the 24-char weak
///       comparison falsely matches the all-zero-suffix row.  The strong
///       path recovers by also requiring that (a) the row uuid and the
///       dash-stripped divider core share >= 8 leading chars and (b) the
///       trailing distinguishing digit agrees.  A STRONG match always wins
///       over any WEAK match.
///
/// CompactGroup rows NEVER match (they carry no uuid — the first payload row
/// of the post-divider section will match instead, which is the correct
/// behaviour: a divider placed inside a collapsed group still shows up, and
/// clicking "expand" reveals the group contents with the divider still
/// sitting before the exact row that was unseen).
[[nodiscard]] auto find_divider_before_visible_index(
    const MessagesListInput& input,
    const std::vector<VisibleRow>& visible) -> std::size_t;

/// Renders the unseen divider: a left-titled separator with a margin
/// line above.
///
/// The divider itself is a left-titled separator: "─── N new messages ──────"
/// with the title in bold/muted and lines in muted/subtle.
[[nodiscard]] auto render_unseen_divider(std::size_t count) -> Element;

/// Renders a muted separator: "─── N older messages hidden · Ctrl+E to show all ───"
/// Inserted at the top of the visible list when transcript mode caps at 30.
[[nodiscard]] auto render_transcript_cap_divider(std::size_t hidden_count) -> Element;

/// Decide the envelope status badge purely from MessageShape + stream state.
auto derive_status_badge(MessageShape s, std::size_t row_idx,
                         std::size_t streaming_tail)
    -> EnvelopeStatusBadge;

auto render_payload_row(const MessagesListInput& input,
                        std::size_t row_idx,
                        bool is_selected,
                        std::size_t frame_count,
                        bool add_margin) -> Element;

/// Return true if the thinking row at `row_idx` was the streaming tail
/// within the 3s collapse grace — i.e. it should still be rendered
/// expanded.  Defined in messages_list_payload_row.cpp; called by
/// estimate_row_height (messages_list_geometry.cpp) to size expanded
/// thinking rows correctly in the virtual-list geometry.
[[nodiscard]] bool was_recently_streaming(std::size_t row_idx);

auto render_compact_group_row(const VisibleRow& vr,
                              bool is_selected) -> Element;

/// Render a compressed thinking+tool chain as a single dim summary line.
/// Live chains (query still streaming) show a spinner + elapsed timer +
/// present-continuous tool activity; static chains show "Thought for X, …".
auto render_compressed_chain_row(const VisibleRow& vr,
                                 bool is_selected,
                                 std::size_t frame_count = 0) -> Element;

/// Returns a lowercase copy of the search query (if any) — used to highlight
/// matched substrings in render output.  (Currently used for the empty-state
/// copy; real per-row substring highlighting is a UI17 deliverable.)
auto render_empty_state(const std::string& search_query) -> Element;

} // namespace detail

// =========================================================================
// 7)  STATIC RENDER  (render_messages_list_view)
// =========================================================================
/// Non-interactive version.  Used by dialogs that don't need the full
/// selection/action pipeline.  Still applies filters + compact collapsing
/// + streaming tail rendering + scroll frame.

constexpr std::size_t kMaxRenderedLastN = 80;   // last-N render cap

/// @param wrap_in_yframe When true (default), wraps the message rows in
///        yframe | vscroll_indicator | flex.  When false, returns just the
///        bare vbox of rows — the caller is responsible for wrapping in a
///        yframe (used by RenderReplScreen which wraps Logo + messages +
///        filler + Spinner in ONE yframe).
/// @param trailing_elements Optional elements to append after the message rows
///        INSIDE the yframe.  Used by RenderReplScreen to inject the elastic
///        filler so it absorbs remaining viewport
///        space without competing with yframe|flex for parent allocation.
///        Golden tests leave this empty.
[[nodiscard]] auto render_messages_list_view(
    const MessagesListInput& input_const,
    std::size_t frame_count = 0,
    std::size_t render_last_n = kMaxRenderedLastN,
    Elements trailing_elements = {},
    bool wrap_in_yframe = true,
    Elements leading_elements = {}) -> Element;

// =========================================================================
// 8)  INTERACTIVE COMPONENT  (MakeMessagesList)
// =========================================================================
/// Full FTXUI Component.  Implements:
///   • j / k / Ctrl+N / Ctrl+P  — move selection up / down
///   • g / Home  — first row ;  G / End  — last row
///   • /         — focus embedded search box
///   • Escape    — clear search query, then (if already clear) deselect
///   • Enter     — "default action" on selected row = Copy
///   • c         — copy ;  r — regenerate ;  d — delete
///   • Space     — toggle compact-group expand/collapse (if on a group row)
///
/// The search Input widget is rendered in a fixed header row stacked ABOVE
/// the scrolling vbox — exactly the same layout pattern as UI6.

class MessagesListComponent final : public ComponentBase {
  public:
    MessagesListComponent(MessagesListInput input,
                          MessagesListCallbacks callbacks);

    // ── Event handling ────────────────────────────────────────────────
    bool OnEvent(Event event) override;

    // ── Rendering ------------------------------------------------------
    Element Render() override;

    // ── Public setters — callers use these between frames to feed
    //    streaming deltas, filter toggles, etc. --------------------------

    void set_input(MessagesListInput next);

    auto& input()       noexcept { return input_; }
    auto& input() const noexcept { return input_; }

  private:
    // ── Internals ------------------------------------------------------
    void rebuild_visible_cache();

    auto filter_hash() const -> std::uint64_t;

    void move_selection(int delta);

    void move_selection_to(std::size_t abs_idx);

    void commit_selection(std::size_t visible_idx);

    auto current_visible_row() -> const VisibleRow*;

    auto fire_action(ActionKind k) -> bool;

    // ── Members --------------------------------------------------------
    MessagesListInput    input_;
    MessagesListCallbacks cbs_;

    Component            search_input_;
    std::string          live_search_query_;

    std::vector<VisibleRow> visible_rows_;
    std::optional<std::size_t> selected_visible_index_;
    std::size_t          frame_count_ = 0;

    // Change-detection cache keys
    std::size_t          last_rows_size_   = std::size_t(-1);
    std::string          last_search_;
    std::uint64_t        last_filter_hash_ = std::uint64_t(-1);

    // ── Mouse click-to-expand tracking ──────────────────────────────────
    // Each frame, Render() records the screen box of every visible row via
    // reflect().  OnEvent() uses these to map a mouse click → visible row,
    // then toggles its expansion (same logic as Space/Enter keys).
    //
    // tracked_boxes_ uses unique_ptr<Box> because reflect() takes a Box&
    // and vector<Box> reallocation would invalidate references during the
    // row-building loop (push_back may grow the vector).
    std::vector<std::size_t>        tracked_vi_;       // visible index per row
    std::vector<std::unique_ptr<Box>> tracked_boxes_;  // screen boxes (parallel)
    std::optional<std::size_t>      hovered_vi_;       // mouse-hovered row index
};

[[nodiscard]] auto MakeMessagesList(
    MessagesListInput input,
    MessagesListCallbacks callbacks = {}) -> Component;

// =========================================================================
// Test seams
// =========================================================================
// RFC 0003: clear the process-global thinking-stream grace map
// (messages_list_payload_row.cpp) so streaming-replay fixtures don't leak
// state across tests. For testing only.
void clear_thinking_stream_last_seen_for_testing();

} // namespace loom::ui::messages_list

// =========================================================================
// REPL integration note:
//   In repl_screen.cppm's `RenderMessages` / `MakeReplScreen` the call site
//   uses this shape/payload API once the engine owns the parallel vectors:
//
//     import loom.ui.messages.messages_list;
//     using loom::ui::messages_list::MakeMessagesList;
//     using loom::ui::messages_list::MessagesListInput;
//     using loom::ui::messages_list::MessagesListCallbacks;
//
//     MessagesListInput in;
//     in.rows   = engine.message_payloads();   // vector<MessageRowPayload>
//     in.shapes = engine.message_shapes();     // vector<MessageShape>
//     in.selected_row_idx     = state.selected_message_idx >= 0 ?
//                                 std::optional(state.selected_message_idx) :
//                                 std::nullopt;
//     in.streaming_tail_row   = engine.is_streaming() ?
//                                 (in.rows.empty() ? 0 : in.rows.size() - 1)
//                               : in.rows.size();   // "not streaming" sentinel
//     in.compact_boundary_groups = engine.compact_groups();
//     in.filters              = {state.show_system, state.show_tool_in, …};
//     in.search_query         = state.search_query;
//     in.jump_to_row_on_init  = state.jump_row;
//
//     MessagesListCallbacks cb{
//         .on_select = [](size_t i){ state.selected_message_idx = int(i); },
//         .on_action = [](size_t i, ActionKind k){ engine.on_row_action(i,k); },
//         .on_toggle_compact_group  = [](size_t g){ engine.toggle_group(g); },
//         .on_click_attachment      = … ,
//         .on_search_changed        = [&](auto& q){ state.search_query = q; },
//     };
//     auto list = MakeMessagesList(std::move(in), std::move(cb));
//
//   Use Render() in the non-interactive path:
//     auto elm = loom::ui::messages_list::render_messages_list_view(in, n, 80);
// =========================================================================
