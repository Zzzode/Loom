// messages_list_geometry.cpp - impl unit for loom.ui.messages.messages_list
// (RFC 0001 Phase C batch 7). Per-row line-height heuristics and the
// VisibleRow <-> virtual_list::VisibleRow encoding (backend_index bit pack).
//
// Phase A (#184957): std is TEXTUAL here on purpose — no `import std;`.
// With an empty GMF, this impl unit of a primary whose own GMF pulls libc++
// textually (via FTXUI) made clang 22's reduced-BMI writer emit a duplicate
// aligned operator new ("call to 'operator new' is ambiguous" in
// std::__libcpp_allocate). Impl units that textually include FTXUI
// themselves are unaffected; this one names no FTXUI type, so the
// app_extra_methods.cpp fallback applies.
module;

#include <cstddef>
#include <cstdint>

module loom.ui.messages.messages_list;

import std;

import loom.ui.messages.message_row;
import loom.ui.messages.virtual_list;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.local_command_output_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.user_text_message;

namespace loom::ui::messages_list {

namespace detail {

/// Per-content-block height cap.  A single paragraph or code block longer
/// than this is reported at the cap — the virtual list's JumpHandle geometry
/// will underestimate the row, but the real renderer still paints full
/// content when the row enters the viewport slice.  500 lines covers
/// realistic code blocks and pastes while preventing pathological geometry
/// from a 100K-char single-line blob.
constexpr int kMaxContentLines = 500;

/// Per-row total height cap (content + envelope header + separator).
/// Generous enough for the largest realistic message rows.
constexpr int kMaxRowHeight = 1000;

/// Count *wrapped* lines for `text` given terminal columns.  Uses a
/// text-wrap heuristic (hard-break at term_cols, plus existing '\n').  The
/// result is the maximum vertical space the content COULD take inside a
/// 36-col reserved left-gutter message envelope; 36 is subtracted from
/// term_cols to account for the fixed avatar column.
[[nodiscard]] auto estimate_content_lines(
    std::string_view text,
    int term_cols,
    int envelope_gutter_cols) -> int {
    const int content_cols =
        std::max(20, term_cols - envelope_gutter_cols);
    int lines = 1;
    int current = 0;
    for (char c : text) {
        if (c == '\n') {
            lines += 1;
            current = 0;
            continue;
        }
        current += 1;
        if (current > content_cols) {
            lines += 1;
            current = 0;
        }
    }
    // Clip to [1, kMaxContentLines] — rows taller than the cap are reported
    // as the cap.  The real renderer still paints full content when the row
    // enters the viewport; only the JH scroll geometry is underestimated.
    return std::clamp(lines, 1, kMaxContentLines);
}

/// Normalize text according to CommonMark soft/hard break rules (§6.1):
/// "  \n" or "\\\n" → hard break; regular "\n" → soft break (joined with
/// space); blank line → paragraph boundary (collapsed to \n\n).  Leading/
/// trailing blank runs are stripped.  Leading spaces on continuation
/// lines are stripped.  Matches the Markdown renderer's
/// split_on_hard_breaks (called per paragraph by the lexer).
///
/// Code-block aware: fenced (```/~~~) and indented (4+ spaces after blank
/// line) code blocks preserve line breaks (joined with \n, not space) so
/// the estimator counts each code line as a separate row — matching the
/// renderer's render_code_block which emits one vbox row per source line.
[[nodiscard]] auto normalize_markdown_breaks(std::string_view text)
    -> std::string {
    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;
    bool seen_content = false;
    // Tracks whether the previous line was blank (paragraph boundary).
    // Used to detect indented code blocks, which CommonMark requires to
    // be preceded by a blank line (cannot interrupt a paragraph).
    bool after_blank = false;
    // Fenced code block state: when non-zero, we are inside a fenced code
    // block delimited by this many backticks or tildes.
    int fence_len = 0;
    char fence_char = '\0';
    // Indented code block state.
    bool in_indented_code = false;

    // Helper: check if a line is a fenced code opener.  Returns fence_len
    // (>=3) and sets fc/fname, or 0 if not an opener.
    auto detect_fence = [](std::string_view line, char& fc) -> int {
        std::size_t lead = 0;
        while (lead < line.size() && line[lead] == ' ' && lead < 3)
            ++lead;
        if (lead >= line.size()) return 0;
        const char c = line[lead];
        if (c != '`' && c != '~') return 0;
        std::size_t len = 0;
        while (lead + len < line.size() && line[lead + len] == c)
            ++len;
        if (len < 3) return 0;
        // For backtick fences, info string must not contain backtick.
        if (c == '`') {
            for (std::size_t k = lead + len; k < line.size(); ++k) {
                if (line[k] == '`') return 0;
            }
        }
        fc = c;
        return static_cast<int>(len);
    };

    // Helper: check if a line is a closing fence for the given char/len.
    auto is_closing_fence = [](std::string_view line, char fc, int fl) -> bool {
        std::size_t lead = 0;
        while (lead < line.size() && line[lead] == ' ' && lead < 3)
            ++lead;
        if (lead >= line.size() || line[lead] != fc) return false;
        std::size_t len = 0;
        while (lead + len < line.size() && line[lead + len] == fc)
            ++len;
        if (len < static_cast<std::size_t>(fl)) return false;
        // The closing fence may be followed only by spaces (CommonMark
        // §4.5); a non-whitespace tail means the line is code content.
        for (std::size_t k = lead + len; k < line.size(); ++k) {
            if (line[k] != ' ' && line[k] != '\t') return false;
        }
        return true;
    };

    // Helper: count leading spaces.
    auto leading_spaces = [](std::string_view line) -> int {
        int n = 0;
        while (n < static_cast<int>(line.size()) && line[n] == ' ')
            ++n;
        return n;
    };

    // Helper: is line blank (empty or whitespace-only).
    auto is_blank_line = [](std::string_view line) -> bool {
        for (char c : line) {
            if (c != ' ' && c != '\t') return false;
        }
        return true;
    };

    // Ensure the result ends with a \n\n block boundary, matching the
    // lexer's flush_paragraph() before a fenced opener and the renderer's
    // blank separator row between consecutive block tokens.
    auto ensure_block_boundary = [&]() {
        if (!seen_content || result.empty()) return;
        while (!result.empty() && result.back() == ' ')
            result.pop_back();
        if (!result.empty() && result.back() != '\n')
            result += '\n';
        if (result.size() < 2 ||
            result[result.size() - 1] != '\n' ||
            result[result.size() - 2] != '\n') {
            result += '\n';
        }
    };

    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
            ? text.substr(pos)
            : text.substr(pos, nl - pos);
        const bool last_line = (nl == std::string_view::npos);

        // ── Fenced code block ──────────────────────────────────────────
        if (fence_len > 0) {
            if (is_closing_fence(line, fence_char, fence_len)) {
                fence_len = 0;
                fence_char = '\0';
                after_blank = true;
                // Block separator after the code block (renderer inserts
                // a blank row between consecutive block tokens).
                ensure_block_boundary();
            } else {
                if (!result.empty() && result.back() != '\n')
                    result += '\n';
                result += line;
                seen_content = true;
                after_blank = false;
            }
            if (last_line) break;
            pos = nl + 1;
            continue;
        }

        // ── Indented code block ────────────────────────────────────────
        if (in_indented_code) {
            if (is_blank_line(line)) {
                // Peek ahead: include blank only if next non-blank line
                // is also indented (part of the code block).
                std::size_t peek = last_line ? std::string_view::npos
                                              : nl + 1;
                bool include_blank = false;
                bool end_code = false;
                while (peek < text.size()) {
                    const std::size_t pnl = text.find('\n', peek);
                    std::string_view pline = (pnl == std::string_view::npos)
                        ? text.substr(peek)
                        : text.substr(peek, pnl - peek);
                    if (!is_blank_line(pline)) {
                        if (leading_spaces(pline) >= 4)
                            include_blank = true;
                        else
                            end_code = true;
                        break;
                    }
                    peek = (pnl == std::string_view::npos)
                        ? std::string_view::npos : pnl + 1;
                }
                if (peek == std::string_view::npos)
                    end_code = true;

                if (end_code) {
                    // End code block.  The blank line is a paragraph
                    // boundary — fall through to normal blank handling.
                    in_indented_code = false;
                    after_blank = true;
                    // Reprocess this line as a normal blank line.
                } else if (include_blank) {
                    result += '\n';
                    if (last_line) break;
                    pos = nl + 1;
                    continue;
                } else {
                    // Blank at end of code block — end it.
                    in_indented_code = false;
                    after_blank = true;
                    // Reprocess as normal blank.
                }
            } else if (leading_spaces(line) >= 4) {
                result += '\n';
                result += line.substr(4);
                seen_content = true;
                after_blank = false;
                if (last_line) break;
                pos = nl + 1;
                continue;
            } else {
                // End of code block — non-indented content starts a new
                // block.  The lexer emits the code block and the following
                // paragraph as separate tokens; the renderer inserts a
                // blank separator row.
                in_indented_code = false;
                after_blank = false;
                ensure_block_boundary();
                // Fall through to normal line handling.
            }
        }

        // ── Blank line ─────────────────────────────────────────────────
        if (is_blank_line(line)) {
            if (last_line) break;
            if (!seen_content) {
                // Leading blank can precede an indented code block
                // (CommonMark: indented code blocks may start at the
                // beginning of the document).
                after_blank = true;
                pos = nl + 1;
                continue;
            }
            while (!result.empty() && result.back() == ' ')
                result.pop_back();
            if (!result.empty() && result.back() != '\n')
                result += '\n';
            if (result.size() < 2 ||
                result[result.size() - 1] != '\n' ||
                result[result.size() - 2] != '\n') {
                result += '\n';
            }
            after_blank = true;
            pos = nl + 1;
            continue;
        }

        seen_content = true;

        // ── Fenced code block opener ───────────────────────────────────
        {
            char fc = '\0';
            int fl = detect_fence(line, fc);
            if (fl > 0) {
                // Block separator before the code block: the lexer flushes
                // the current paragraph before a fenced opener, and the
                // renderer inserts a blank separator row between blocks.
                ensure_block_boundary();
                fence_len = fl;
                fence_char = fc;
                after_blank = false;
                if (last_line) break;
                pos = nl + 1;
                continue;
            }
        }

        // ── Indented code block start (4+ spaces after blank) ──────────
        if (after_blank && leading_spaces(line) >= 4) {
            in_indented_code = true;
            after_blank = false;
            if (!result.empty() && result.back() != '\n')
                result += '\n';
            result += line.substr(4);
            if (last_line) break;
            pos = nl + 1;
            continue;
        }

        // ── Normal line (soft/hard break) ──────────────────────────────
        after_blank = false;

        // Strip leading spaces for paragraph continuation (CommonMark
        // trims paragraph indentation).  This runs AFTER the indented
        // code block check so code blocks can see the indentation.
        while (!line.empty() && line[0] == ' ')
            line.remove_prefix(1);

        bool hard = false;
        if (!last_line) {
            if (line.size() >= 2 &&
                line[line.size() - 1] == ' ' &&
                line[line.size() - 2] == ' ') {
                hard = true;
                while (!line.empty() && line.back() == ' ')
                    line.remove_suffix(1);
            } else if (!line.empty() && line.back() == '\\') {
                hard = true;
                line.remove_suffix(1);
            }
        }

        result += line;

        if (!last_line) {
            if (hard) {
                result += '\n';
            } else {
                while (!result.empty() && result.back() == ' ')
                    result.pop_back();
                result += ' ';
            }
            pos = nl + 1;
        } else {
            break;
        }
    }
    while (result.size() >= 2 &&
           result[result.size() - 1] == '\n' &&
           result[result.size() - 2] == '\n') {
        result.pop_back();
        result.pop_back();
    }
    return result;
}

/// Word-boundary wrap variant of estimate_content_lines.  Matches the
/// Markdown renderer's flexbox word-wrap (atomic words, break at spaces)
/// with CommonMark soft/hard break semantics; produces >=
/// estimate_content_lines for the same text.  Used for AssistantText
/// where the faithful renderer wraps at word boundaries.
[[nodiscard]] auto estimate_content_lines_word(
    std::string_view text,
    int term_cols,
    int envelope_gutter_cols) -> int {
    const std::string normalized = normalize_markdown_breaks(text);
    const int content_cols =
        std::max(20, term_cols - envelope_gutter_cols);
    int lines = 0;
    std::size_t pos = 0;
    while (pos < normalized.size()) {
        std::size_t nl = normalized.find('\n', pos);
        if (nl == std::string_view::npos) nl = normalized.size();
        const std::string_view line(normalized.data() + pos, nl - pos);

        int current = 0;
        std::size_t i = 0;
        bool has_content = false;
        while (i < line.size()) {
            while (i < line.size() && line[i] == ' ') ++i;
            if (i >= line.size()) break;
            std::size_t word_end = i;
            while (word_end < line.size() && line[word_end] != ' ') ++word_end;
            const int word_len = static_cast<int>(word_end - i);
            if (word_len > content_cols) {
                if (current > 0) { ++lines; current = 0; }
                lines += (word_len + content_cols - 1) / content_cols;
            } else {
                if (current > 0 && current + 1 + word_len > content_cols) {
                    ++lines;
                    current = word_len;
                } else {
                    if (current > 0) current += 1;
                    current += word_len;
                }
            }
            has_content = true;
            i = word_end;
        }
        if (current > 0 || !has_content) ++lines;

        pos = nl + 1;
    }
    // No trailing-\n extra line: the normalizer converts soft breaks to
    // spaces, so a trailing \n only comes from a hard break at EOF — and
    // render_inlines() does not push an empty row after a trailing
    // HardBreakNode.
    return std::clamp(lines, 1, kMaxContentLines);
}

/// Estimate visual height for one messages_list::VisibleRow.  Adds 1 line
/// for the envelope's top-accent + role-header row and 1 for trailing
/// separator (except for 1-line rows where it collapses).
[[nodiscard]] auto estimate_row_height(
    const VisibleRow& vr,
    const MessagesListInput& input,
    int term_cols) -> int {
    using K = VisibleRow::Kind;
    if (vr.kind == K::CompactGroup) {
        // Collapsed "📦 27 messages collapsed (📦 8 tool turns, +++12 ---7)"
        return 1;
    }
    if (vr.kind == K::CompressedChain) {
        // 1-line top margin + 1-line summary
        return 2;
    }
    if (vr.kind == K::TranscriptCapDivider) {
        // "─── N older messages hidden · Ctrl+E to show all ───"
        return 1;
    }
    // Payload rows — dispatch by MessageShape content length.
    if (vr.row_idx >= input.rows.size()) return 2;
    const std::string preview =
        detail::lowered(detail::payload_preview(input.rows[vr.row_idx]));
    const MessageShape shape =
        vr.row_idx < input.shapes.size()
            ? input.shapes[vr.row_idx]
            : MessageShape::SystemTaskAssignment;

    using S = MessageShape;
    int content_lines = 1;
    switch (shape) {
        case S::AssistantThinking:
        case S::AssistantRedactedThinking: {
            // Collapsed thinking renders a 1-line label ("∴ Thought for
            // Xs <summary> (ctrl+o to expand)").  Expanded thinking
            // (streaming tail, within the 3s collapse grace, or transcript
            // mode) renders the full body — potentially dozens of lines.
            // Estimate from the actual content when expanded so the
            // virtual-list JumpHandle geometry matches the rendered height;
            // a 2-line estimate for a 30-line expanded row causes the
            // visible slice + spacers to be wildly wrong (content
            // overlapping / "compressed" during streaming).
            const bool is_expanded =
                (vr.row_idx == input.streaming_tail_row) ||
                input.is_transcript_mode ||
                was_recently_streaming(vr.row_idx);
            if (is_expanded) {
                if (auto* opts = std::get_if<thinking_message::ThinkingMessageOptions>(
                        &input.rows[vr.row_idx])) {
                    std::string full_text = opts->data.raw_text;
                    if (full_text.empty() && !opts->data.sections.empty()) {
                        for (const auto& s : opts->data.sections) {
                            if (!full_text.empty()) full_text.push_back('\n');
                            full_text += s.content;
                        }
                    }
                    if (!full_text.empty()) {
                        // label (1) + body + top/bottom margin (2)
                        content_lines =
                            estimate_content_lines(full_text, term_cols, 4) + 3;
                    } else {
                        content_lines = 2;
                    }
                } else {
                    content_lines = 2;
                }
            } else {
                content_lines = 2;
            }
            break;
        }
        case S::AssistantToolUse:
        case S::AssistantGroupedTools: {
            if (auto* topts = std::get_if<tool_use_message::ToolUseRenderOptions>(
                    &input.rows[vr.row_idx])) {
                const auto& call = topts->call;
                // Resolved built-in tools render as just a 1-line header
                // (● ToolName (command)) — no Input/Output sections.
                const bool is_resolved =
                    (call.status == tool_use_message::ToolStatus::Success ||
                     call.status == tool_use_message::ToolStatus::Error ||
                     call.status == tool_use_message::ToolStatus::Cancelled);
                if (is_resolved) {
                    content_lines = 1;
                } else {
                    // Running/Pending: header + progress line
                    content_lines = 2;
                }
            } else if (auto* gopts = std::get_if<tool_use_message::GroupedToolsOptions>(
                           &input.rows[vr.row_idx])) {
                int visible = std::min(
                    static_cast<int>(gopts->calls.size()),
                    gopts->max_visible_preview);
                content_lines = 2 + visible;
            } else {
                content_lines = 2;
            }
            break;
        }
        case S::UserToolResult:
        case S::UserBashOutput: {
            // payload_preview returns just tool_name
            // (e.g. "Bash") — 1 line.  Actual tool result output can be
            // dozens of lines.  Extract real output from ToolResultOptions.
            if (auto* ropts = std::get_if<ToolResultOptions>(
                    &input.rows[vr.row_idx])) {
                std::string full_text;
                if (ropts->content_items && !ropts->content_items->empty()) {
                    // Structured content items (MCP tools): concatenate text.
                    for (const auto& item : *ropts->content_items) {
                        if (item.type == "text") {
                            if (!full_text.empty()) full_text += '\n';
                            full_text += item.text;
                        } else if (item.type == "image") {
                            if (!full_text.empty()) full_text += '\n';
                            full_text += "[Image]";
                        }
                    }
                } else if (ropts->output && !ropts->output->empty()) {
                    full_text = *ropts->output;
                } else if (ropts->error_message && !ropts->error_message->empty()) {
                    full_text = *ropts->error_message;
                }
                if (!full_text.empty()) {
                    content_lines = estimate_content_lines(full_text, term_cols, 4);
                } else {
                    content_lines = 2;  // minimal: header + "(no output)"
                }
                // +1 for header row (status icon + tool name + duration)
                content_lines += 1;
                if (ropts->is_truncated) content_lines += 1;  // "(output truncated)"
            } else {
                // Fallback for UserBashOutput (BashIOEntry variant)
                content_lines = estimate_content_lines(preview, term_cols, 4);
            }
            break;
        }
        case S::UserLocalCommandOutput:
            // Command output (e.g. /help, /theme list) can be dozens of
            // lines.  payload_preview returns just "local-command" for this
            // variant, so we must count actual output lines from the payload.
            if (auto* opts = std::get_if<local_cmd::LocalCommandOptions>(
                    &input.rows[vr.row_idx])) {
                // Each OutputLine is one display row; add header + footer.
                content_lines = static_cast<int>(opts->data.lines.size()) + 3;
            } else {
                content_lines = estimate_content_lines(preview, term_cols, 4);
            }
            break;
        case S::UserLocalJsxOutput:
            // JSX overlay output (e.g. /skills, /agents) renders each line
            // as a separate text() row — one display row per \n.  The
            // payload is UserTextMessageData whose content_preview carries
            // the full text, so estimate_content_lines counts correctly.
            content_lines = estimate_content_lines(preview, term_cols, 0);
            break;
        case S::SystemText:
        case S::SystemRateLimit:
        case S::SystemPlanApproval:
        case S::SystemHookProgress:
        case S::SystemShutdown:
        case S::SystemAdvisor:
        case S::SystemTaskAssignment:
        case S::SystemAPIError:
        case S::SystemCollapsedContent:
        case S::SystemCompactBoundary:
            content_lines = 1 + estimate_content_lines(preview, term_cols, 4) / 2;
            break;
        case S::UserBashInput:
            // "> echo hello" prompt-style — short.
            content_lines = 2;
            break;
        case S::UserImage:
            content_lines = 4;   // label + metadata + source (no fake thumbnail)
            break;
        case S::UserAttachments:
            content_lines = 3;   // grid header + 1 row of thumbs
            break;
        case S::UserText:
        case S::UserPrompt: {
            // UserTextMessage wraps at a fixed 76 cols and truncates
            // content > 10K chars to head 2500 + tail 2500 before
            // rendering.  Apply the same truncation and wrap width so
            // the virtual-list estimate matches the rendered height.
            const std::string truncated =
                ::loom::ui::messages::TruncateUserPromptText(preview);
            content_lines = 1 + estimate_content_lines(truncated, 76, 0);
            break;
        }
        case S::UserCommand:
            // Compact 1-line chip ("/command args") — no truncation,
            // no wrapping.  RenderUserCommandMessage collapses to
            // content width.
            content_lines = 1;
            break;
        case S::AssistantText:
            // Faithful renderer reserves 2 cols for the "●" bullet glyph;
            // the body wraps at term_cols-2 via Markdown flexbox
            // word-boundary wrapping.  Use the word-wrap estimator (not
            // hard-wrap) to match — hard-wrap underestimates for text
            // with spaces, making the last line unreachable at bottom.
            content_lines = 1 + estimate_content_lines_word(preview, term_cols, 2);
            break;
        default:
            content_lines = 1 + estimate_content_lines(preview, term_cols, 36);
            break;
    }
    // +1 line for envelope header (avatar + role pill) unless the shape
    // uses the faithful renderer which bypasses that envelope entirely.
    switch (shape) {
        case S::AssistantThinking:
        case S::AssistantRedactedThinking:
        case S::AssistantText:
        case S::UserText:
        case S::UserPrompt:
        case S::UserCommand:
        case S::UserLocalJsxOutput:
        case S::SystemText:
        case S::SystemRateLimit:
        case S::SystemPlanApproval:
        case S::SystemHookProgress:
        case S::SystemShutdown:
        case S::SystemAdvisor:
        case S::SystemTaskAssignment:
        case S::SystemAPIError:
        case S::SystemCompactBoundary:
        case S::SystemCollapsedContent:
            break;   // faithful renderer: no avatar/role-pill header
        default:
            content_lines += 1;
    }
    return std::clamp(content_lines, 1, kMaxRowHeight);
}

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
    int term_cols)
    -> std::vector<loom::ui::messages::virtual_list::VisibleRow>
{
    namespace vl = loom::ui::messages::virtual_list;
    std::vector<vl::VisibleRow> out;
    out.reserve(visible.size());
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const auto& vr = visible[i];
        const int est = detail::estimate_row_height(vr, input, term_cols);

        // Encode kind into backend_index MSBs for round-trip via
        // decode_virtual_backend_index.  Bit 63 = CompactGroup,
        // bit 62 = TranscriptCapDivider, bit 61 = CompressedChain,
        // neither = Payload.
        constexpr std::uint64_t kGroupBit  = std::uint64_t(1) << 63;
        constexpr std::uint64_t kCapBit    = std::uint64_t(1) << 62;
        constexpr std::uint64_t kChainBit  = std::uint64_t(1) << 61;
        std::uint64_t backend;
        int type_hint;

        if (vr.kind == VisibleRow::Kind::CompactGroup) {
            backend   = kGroupBit | static_cast<std::uint64_t>(vr.group_idx);
            type_hint = 1;
        } else if (vr.kind == VisibleRow::Kind::TranscriptCapDivider) {
            backend   = kCapBit | static_cast<std::uint64_t>(vr.hidden_count);
            type_hint = 2;
        } else if (vr.kind == VisibleRow::Kind::CompressedChain) {
            backend   = kChainBit | static_cast<std::uint64_t>(vr.group_count);
            type_hint = 3;
        } else {
            backend   = static_cast<std::uint64_t>(vr.row_idx);
            type_hint = 0;
        }

        vl::VisibleRow row{
            .row_id                 = static_cast<std::uint64_t>(i) + 1,
            .estimated_height_lines = est,
            .height_measured        = false,
            .search_key             = {},
            .type_hint              = type_hint,
            .backend_index          = backend,
        };
        if (row.type_hint == 0 && vr.row_idx < input.rows.size()) {
            // Populate search_key using the cached lowered rich text.
            // If build_visible_rows already warmed the cache (search was
            // active), this is a zero-alloc cache hit.  Otherwise this
            // call computes + caches for future use.
            //
            // The search callback is the same one used by build_visible_rows
            // search filter.
            const MessageShape shape =
                vr.row_idx < input.shapes.size()
                    ? input.shapes[vr.row_idx]
                    : MessageShape::SystemTaskAssignment;
            row.search_key = detail::get_cached_lowered_search_text(
                input, vr.row_idx, shape);
        }
        out.push_back(std::move(row));
    }
    return out;
}

/// Decode the backend_index set by `visible_rows_to_virtual` back into a
/// messages_list::VisibleRow.  `out` is filled in place; returns true if
/// decode succeeded, false if the key was malformed (out is reset to a
/// safe payload(0) sentinel on failure).
[[nodiscard]] bool decode_virtual_backend_index(
    std::uint64_t backend_index,
    VisibleRow& out) noexcept
{
    constexpr std::uint64_t kGroupBit = std::uint64_t(1) << 63;
    constexpr std::uint64_t kCapBit   = std::uint64_t(1) << 62;
    constexpr std::uint64_t kChainBit = std::uint64_t(1) << 61;
    if ((backend_index & kGroupBit) != 0) {
        out.kind      = VisibleRow::Kind::CompactGroup;
        out.group_idx = backend_index & (~kGroupBit);
        out.row_idx   = 0;
        return true;
    }
    if ((backend_index & kCapBit) != 0) {
        out.kind         = VisibleRow::Kind::TranscriptCapDivider;
        out.hidden_count = backend_index & (~kCapBit);
        out.row_idx      = 0;
        out.group_idx    = 0;
        return true;
    }
    if ((backend_index & kChainBit) != 0) {
        out.kind        = VisibleRow::Kind::CompressedChain;
        out.group_count = backend_index & (~kChainBit);
        out.row_idx     = 0;
        out.group_idx   = 0;
        return true;
    }
    out.kind    = VisibleRow::Kind::Payload;
    out.row_idx = static_cast<std::size_t>(backend_index);
    out.group_idx = 0;
    return true;
}

} // namespace loom::ui::messages_list
