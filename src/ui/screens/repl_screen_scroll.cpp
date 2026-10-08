// repl_screen_scroll.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). unseen-divider computation, visible-message projection, transcript row
// estimation and scroll bounds (incl. the virtual_list JumpHandle fast path).
//
// Phase A (#184957): std is TEXTUAL here on purpose - no `import std;`.
// Empty-GMF impl unit of an FTXUI-GMF primary: clang 22's reduced-BMI
// writer would otherwise emit a duplicate aligned operator new
// (LLVM #184957). Same fallback as app_extra_methods.cpp /
// messages_list_geometry.cpp.
module;

#include <cstddef>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.messages.messages_list;
import loom.ui.messages.virtual_list;

namespace loom::ui::repl_screen {

// ── UnseenDivider helpers ──────────────────────────────────────────────
// Counts new assistant turns that arrived after the user scrolled away
// from bottom, and builds the UnseenDivider struct passed to RenderMessages.

namespace unseen_detail {

/// Whether an assistant entry has visible text content.  Tool-use-only
/// and thinking-only entries don't count as "new messages" to the user.
[[nodiscard]] bool assistant_has_visible_text(
    const MessageDisplayEntry& e) {
    if (e.role != "assistant") return false;
    // Tool-use entries: never have visible text content to the user.
    // Tool-use blocks are not 'text' type, so they fail the check.
    if (e.is_tool_use) return false;
    // Thinking entries: the check is for b.type === 'text', not 'thinking'.
    // Thinking blocks don't count as visible text for turn counting.
    if (e.is_thinking) return false;
    // If we reach here, it's an assistant text entry with content.
    return !e.content_preview.empty();
}

/// Count assistant turns in entries[start_idx..end).  A "turn" is a
/// non-assistant→assistant transition, skipping system/progress rows
/// and tool-use-only entries.
[[nodiscard]] std::size_t count_unseen_assistant_turns(
    const std::vector<MessageDisplayEntry>& entries,
    std::size_t start_idx) {
    std::size_t count = 0;
    bool prev_was_assistant = false;
    for (std::size_t i = start_idx; i < entries.size(); ++i) {
        const auto& e = entries[i];
        // Skip system rows (progress type)
        if (e.role == "system") continue;
        // Skip tool-use-only assistant entries
        if (e.role == "assistant" && !assistant_has_visible_text(e)) {
            continue;  // don't update prev_was_assistant
        }
        const bool is_assistant = (e.role == "assistant");
        if (is_assistant && !prev_was_assistant) ++count;
        prev_was_assistant = is_assistant;
    }
    return count;
}

}  // namespace unseen_detail

/// Compute the UnseenDivider from state.messages_store.divider_index + messages.
/// Returns nullopt when divider_index is unset, out of range, or no
/// messages have arrived past the divider.
[[nodiscard]] std::optional<::loom::ui::messages_list::UnseenDivider>
ComputeUnseenDivider(const ReplScreenState& s) {
    if (!s.messages_store.divider_index.has_value()) return std::nullopt;
    const auto idx = *s.messages_store.divider_index;
    if (idx >= s.messages_store.messages.size()) return std::nullopt;

    // Find first non-system entry at or after divider_index; the anchor
    // skips progress + null attachments.
    std::size_t anchor_idx = idx;
    while (anchor_idx < s.messages_store.messages.size() &&
           s.messages_store.messages[anchor_idx].role == "system") {
        ++anchor_idx;
    }
    if (anchor_idx >= s.messages_store.messages.size()) return std::nullopt;

    const auto& anchor = s.messages_store.messages[anchor_idx];
    const std::size_t count = std::max(
        std::size_t{1},
        unseen_detail::count_unseen_assistant_turns(s.messages_store.messages, idx));

    ::loom::ui::messages_list::UnseenDivider ud;
    ud.first_unseen_uuid_prefix = anchor.id;
    ud.count = count;
    return ud;
}

[[nodiscard]] const std::vector<MessageDisplayEntry>& BuildVisibleMessages(
    const ReplScreenState& s) {
    if (!s.active_local_jsx_command) {
        // Closing the overlay releases its owned transcript payloads.
        s.messages_store.local_overlay_rows.clear();
        return s.messages_store.messages;
    }
    auto& entries = s.messages_store.local_overlay_rows;
    entries = s.messages_store.messages;

    MessageDisplayEntry command;
    command.role = "user";
    command.is_local_command_input = true;
    command.content_preview = "/" + s.active_local_jsx_command_name;
    if (!s.active_local_jsx_command_args.empty()) {
        command.content_preview += " " + s.active_local_jsx_command_args;
    }
    command.timestamp = std::chrono::system_clock::now();
    entries.push_back(std::move(command));

    std::string content = s.active_local_jsx_content;
    if (!content.empty() && content.back() != '\n') content.push_back('\n');
    if (content.find("Esc to close") == std::string::npos &&
        content.find("Esc to go back") == std::string::npos) {
        content += "\nEsc to close";
    }
    MessageDisplayEntry local_jsx;
    local_jsx.role = "system";
    local_jsx.is_local_jsx_output = true;
    local_jsx.content_preview = std::move(content);
    local_jsx.timestamp = std::chrono::system_clock::now();
    entries.push_back(std::move(local_jsx));

    return entries;
}

[[nodiscard]] int CountTextLines(std::string_view text) {
    if (text.empty()) return 1;
    return static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
}

[[nodiscard]] int EstimateTranscriptRows(
    const std::vector<MessageDisplayEntry>& entries,
    int /*term_cols*/) {
    // NOTE: We deliberately use CountTextLines (newline-only) rather than a
    // wrapping-aware counter here.  content_preview carries the FULL
    // untruncated text for assistant/user rows, so a wrapping-aware count
    // would infl max_offset far past the actual rendered height (markdown
    // rendering, envelope gutters, and FTXUI layout all reduce the real
    // line count).  An over-inflated max_offset makes focusPosition target
    // a non-existent child and freezes the scroll entirely.  The virtual
    // path (visible > 80 rows) uses estimate_row_height which is capped and
    // envelope-aware; this static-path heuristic just needs to stay in the
    // same ballpark, not be exact.
    int rows = 0;
    for (const auto& entry : entries) {
        // content_preview for tool entries is often
        // just a short label ("Bash", "tool-use") while actual rendered
        // content can be dozens of lines.  Extract real content for accurate
        // scroll bounds (fixes "can't scroll to latest message" bug).
        int content_lines = 0;
        if (entry.is_tool_use) {
            // Resolved tools render as 1 line (header only); running/pending
            // as 2 lines (header + progress).  Mirrors estimate_row_height in
            // messages_list_geometry.cpp.  The previous min(CountTextLines, 8)
            // + 3 estimate overestimated resolved tools by 3+ lines each,
            // causing a scroll dead-zone from bottom (N tools × 3 lines ÷
            // 3 lines/notch = N dead wheel notches).
            const std::string& status = entry.tool_status.value_or("pending");
            const bool is_resolved =
                (status == "success" || status == "error" ||
                 status == "cancelled");
            content_lines = is_resolved ? 1 : 2;
        } else if (entry.tool_result_content_items &&
                   !entry.tool_result_content_items->empty()) {
            // Structured tool result (MCP): concatenate text items.
            std::string full;
            for (const auto& item : *entry.tool_result_content_items) {
                if (item.type == "text") {
                    if (!full.empty()) full += '\n';
                    full += item.text;
                } else if (item.type == "image") {
                    if (!full.empty()) full += '\n';
                    full += "[Image]";
                }
            }
            content_lines = CountTextLines(full);
            content_lines += 2;  // header + status row
        } else if (entry.is_image) {
            content_lines = 4;  // label + metadata rows (no fake thumbnail)
        } else if (entry.is_thinking) {
            // Expanded thinking (streaming or within the 3s collapse grace)
            // renders the full body — potentially dozens of lines.  Collapsed
            // thinking renders a 1-line label.  Use full_content for an
            // accurate estimate so scroll bounds (max_offset) match the
            // rendered height; a 2-line estimate for a 30-line expanded row
            // makes max_offset far too small, clamping scroll-away and
            // causing content to jump during streaming.
            if (entry.thinking_active) {
                const std::string& text = entry.full_content.empty()
                    ? entry.content_preview
                    : entry.full_content;
                content_lines = CountTextLines(text) + 3;  // label + body + margins
            } else {
                content_lines = 2;  // collapsed label + separator
            }
        } else {
            content_lines = CountTextLines(entry.content_preview);
        }
        rows += content_lines;
        // Message list inserts one empty separator after each rendered row
        // (top margin from add_margin=true).  Tool results skip this (flush
        // against the preceding tool_use row).
        if (entry.role != "tool") {
            rows += 1;
        }
    }
    return rows;
}

bool ScrollTranscript(const std::shared_ptr<ReplScreenState>& state,
                             int delta) {
    if (!state || delta == 0) return false;

    const int viewport_rows = std::max(1, state->messages_store.viewport_height_lines);

    // P0-3 path: if the VirtualMessageList is active for this frame, use
    // its JumpHandle (prefix-sum table of exact visual lines) for O(log N)
    // scroll bounds instead of the crude EstimateTranscriptRows heuristic.
    if (state->messages_store.virtual_list_active) {
        namespace vl = loom::ui::messages::virtual_list;
        const vl::JumpHandle& jh = state->messages_store.virtual_jh;
        const int total_lines = jh.total();
        if (total_lines <= viewport_rows) return false;
        const int max_top = total_lines - viewport_rows;

        // When pinned to bottom, the view is visually at max_top (via
        // focusPositionRelative in the renderer) but scroll_offset is still
        // 0 — the renderer never writes it back.  Treat the effective
        // position as max_top so wheel-up actually moves the view instead
        // of clamping 0 + (-3) back to 0 (the "slow wheel does nothing"
        // bug).  After the first scroll-away, scroll_offset holds the real
        // position and this branch disengages.
        const bool pinned = state->messages_store.scroll_pinned_to_bottom;
        const int old_top = pinned
            ? max_top
            : std::clamp(state->messages_store.scroll_offset, 0, max_top);
        int target = old_top + delta;
        // Guarantee at least one row moves on PageUp/PageDown style deltas:
        // if target equals old_top, step by one row in the requested
        // direction using binary search.
        if (delta > 0 && target <= old_top) {
            size_t cur = jh.find_row_at_visual_line(old_top);
            if (cur + 1 < jh.size()) target = jh.find_visual_top_for_row(cur + 1);
        } else if (delta < 0 && target >= old_top) {
            size_t cur = jh.find_row_at_visual_line(old_top);
            if (cur > 0) target = jh.find_visual_top_for_row(cur - 1);
        }
        target = std::clamp(target, 0, max_top);
        if (target == old_top) return false;

        // Use scroll_offset=0 as the "pinned to bottom" sentinel (matching
        // ResetScrollToBottom semantics).  Setting offset=max_top here would
        // make the static renderer prioritize the stale absolute focusPosition
        // over pin_to_bottom's focusPositionRelative, causing the view to
        // drift during streaming instead of following the bottom.
        const bool now_pinned = (target >= max_top);
        state->messages_store.scroll_offset = now_pinned ? 0 : target;
        const bool was_pinned = state->messages_store.scroll_pinned_to_bottom;
        state->messages_store.scroll_pinned_to_bottom = now_pinned;

        // On FIRST scroll-away from bottom, snapshot message count as
        // divider_index.  On repin, clear.
        if (was_pinned && !state->messages_store.scroll_pinned_to_bottom) {
            state->messages_store.divider_index = state->messages_store.messages.size();
            state->messages_store.message_count_at_scroll_away = state->messages_store.messages.size();
        } else if (!was_pinned && state->messages_store.scroll_pinned_to_bottom) {
            state->messages_store.divider_index.reset();
            state->messages_store.unseen_divider.reset();
            state->messages_store.unseen_message_count = 0;
            state->messages_store.pill_visible = false;
        }

        // If we are maintaining a live VirtualListState (Component-mode
        // wiring), also update its scroll_top so Render() reuses it.
        if (state->messages_store.virtual_list_state) {
            state->messages_store.virtual_list_state->scroll_top = target;
            vl::update_sticky_after_scroll(*state->messages_store.virtual_list_state,
                                            old_top);
        }
        return true;
    }

    const auto& visible_messages = BuildVisibleMessages(*state);
    if (visible_messages.empty()) return false;
    const int term_cols = std::max(20, state->messages_store.viewport_width_cols);
    const int max_offset =
        std::max(0, EstimateTranscriptRows(visible_messages, term_cols) - viewport_rows);
    if (max_offset == 0) return false;

    // When pinned to bottom, scroll_offset is 0 but the view is visually
    // at max_offset (via focusPositionRelative).  Start from max_offset
    // so wheel-up actually moves the view instead of clamping 0 + (-3)
    // back to 0.
    const int base = state->messages_store.scroll_pinned_to_bottom
        ? max_offset
        : std::clamp(state->messages_store.scroll_offset, 0, max_offset);
    const int next = std::clamp(base + delta, 0, max_offset);
    // scroll_offset=0 is the "pinned" sentinel — see the virtual-path comment
    // above.  Setting offset=max_offset on repin makes the static renderer
    // use a stale absolute focusPosition instead of following the bottom.
    const bool now_pinned = (next >= max_offset);
    state->messages_store.scroll_offset = now_pinned ? 0 : next;
    const bool was_pinned = state->messages_store.scroll_pinned_to_bottom;
    state->messages_store.scroll_pinned_to_bottom = now_pinned;
    if (was_pinned && !state->messages_store.scroll_pinned_to_bottom) {
        state->messages_store.divider_index = state->messages_store.messages.size();
        state->messages_store.message_count_at_scroll_away = state->messages_store.messages.size();
    } else if (!was_pinned && state->messages_store.scroll_pinned_to_bottom) {
        state->messages_store.divider_index.reset();
        state->messages_store.unseen_divider.reset();
        state->messages_store.unseen_message_count = 0;
        state->messages_store.pill_visible = false;
    }
    return true;
}

}  // namespace loom::ui::repl_screen
