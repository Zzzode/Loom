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
import loom.ui.messages.user_text_message;

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

/// Count visual lines after wrapping at term_cols.  Unlike CountTextLines
/// (newline-only), this accounts for long paragraphs that wrap across
/// multiple terminal rows.  Callers that render truncated text (e.g.
/// UserTextMessage's 2500+2500 head/tail truncation) must apply the same
/// truncation before calling this, so the estimate matches the rendered
/// height.
[[nodiscard]] int CountWrappedLines(std::string_view text, int term_cols) {
    if (text.empty()) return 1;
    const int cols = std::max(20, term_cols);
    int lines = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) nl = text.size();
        const std::size_t line_len = nl - pos;
        // Each logical line counts at least 1, even empty ones ("\n" → 2
        // lines, "\n\n" → 3 lines).  Use byte count as a proxy for display
        // width — overestimates for multi-byte UTF-8 (CJK chars are 3 bytes
        // but 2 columns), but overestimation is safer than underestimation
        // for scroll bounds.
        const int visual = static_cast<int>(
            (line_len + static_cast<std::size_t>(cols) - 1)
            / static_cast<std::size_t>(cols));
        lines += std::max(1, visual);
        pos = nl + 1;
    }
    // If text ends with '\n', the trailing empty segment wasn't counted
    // by the loop (pos == text.size() exits before processing it).
    // "\n" → 2 lines, "\n\n" → 3 lines, "a\n" → 2 lines.
    if (!text.empty() && text.back() == '\n') {
        lines += 1;
    }
    return lines;
}

/// Normalize text according to CommonMark soft/hard break rules (§6.1):
/// - "  \n" (two+ trailing spaces) → hard break (strip spaces, keep \n)
/// - "\\\n" (trailing backslash) → hard break (remove backslash, keep \n)
/// - Regular "\n" → soft break (strip trailing spaces, join with space)
/// - Blank line (empty or whitespace-only) → paragraph boundary
///   (collapsed to exactly \n\n; consecutive blanks produce one boundary)
/// - Leading/trailing blank runs are stripped (lexer skips them)
/// - Leading spaces on continuation lines are stripped (matches
///   split_on_hard_breaks in markdown_render_impl.cpp)
///
/// The Markdown lexer splits text into block tokens on blank lines, then
/// calls split_on_hard_breaks per paragraph.  This normalizer replicates
/// that two-level structure so the line count matches the rendered height.
///
/// Code-block aware: fenced (```/~~~) and indented (4+ spaces after blank
/// line) code blocks preserve line breaks (joined with \n, not space) so
/// the estimator counts each code line as a separate row — matching the
/// renderer's render_code_block which emits one vbox row per source line.
[[nodiscard]] std::string NormalizeMarkdownBreaks(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;
    bool seen_content = false;
    bool after_blank = false;
    int fence_len = 0;
    char fence_char = '\0';
    bool in_indented_code = false;

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
        if (c == '`') {
            for (std::size_t k = lead + len; k < line.size(); ++k) {
                if (line[k] == '`') return 0;
            }
        }
        fc = c;
        return static_cast<int>(len);
    };

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

    auto leading_spaces = [](std::string_view line) -> int {
        int n = 0;
        while (n < static_cast<int>(line.size()) && line[n] == ' ')
            ++n;
        return n;
    };

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
                    in_indented_code = false;
                    after_blank = true;
                } else if (include_blank) {
                    result += '\n';
                    if (last_line) break;
                    pos = nl + 1;
                    continue;
                } else {
                    in_indented_code = false;
                    after_blank = true;
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

        // Capture first-content state before seen_content is set, so
        // the indented-code check below can detect document-start blocks.
        const bool first_content = !seen_content;
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

        // ── Indented code block start (4+ spaces after blank or at
        // document start — CommonMark: indented code may begin the
        // document when no paragraph is open) ──────────────────────
        if ((after_blank || first_content) && leading_spaces(line) >= 4) {
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

/// Count visual lines after word-boundary wrapping at term_cols.
/// Unlike CountWrappedLines (hard character wrap), this matches the
/// Markdown renderer's flexbox word-boundary wrapping: words are atomic
/// and lines break at spaces.  Produces >= CountWrappedLines for the same
/// text because it never breaks mid-word, which is the underestimate that
/// made long paragraphs unreachable at the bottom.
///
/// Text is first normalized via NormalizeMarkdownBreaks so that soft
/// breaks (regular \n) are joined with spaces and only hard breaks
/// (two trailing spaces or backslash) force a new line — matching the
/// Markdown renderer's CommonMark §6.1 semantics.
[[nodiscard]] int CountWordWrappedLines(std::string_view text, int term_cols) {
    if (text.empty()) return 1;
    const std::string normalized = NormalizeMarkdownBreaks(text);
    if (normalized.empty()) return 1;  // e.g. "\n" → stripped to empty
    const int cols = std::max(20, term_cols);
    int lines = 0;
    std::size_t pos = 0;
    while (pos < normalized.size()) {
        std::size_t nl = normalized.find('\n', pos);
        if (nl == std::string_view::npos) nl = normalized.size();
        const std::string_view line(normalized.data() + pos, nl - pos);

        // Greedy word-pack: split into words (runs of non-space) and
        // pack into lines of at most `cols` characters.
        int current = 0;
        std::size_t i = 0;
        bool has_content = false;
        while (i < line.size()) {
            while (i < line.size() && line[i] == ' ') ++i;
            if (i >= line.size()) break;
            std::size_t word_end = i;
            while (word_end < line.size() && line[word_end] != ' ') ++word_end;
            const int word_len = static_cast<int>(word_end - i);
            if (word_len > cols) {
                // Overlong atomic segment: hard-break (matches flexbox).
                if (current > 0) { ++lines; current = 0; }
                lines += (word_len + cols - 1) / cols;
            } else {
                if (current > 0 && current + 1 + word_len > cols) {
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
    // No trailing-\n extra line: the normalizer already converts soft
    // breaks to spaces, so a trailing \n only comes from a hard break at
    // EOF — and render_inlines() does not push an empty row after a
    // trailing HardBreakNode (markdown_render_impl.cpp:582-584).
    return lines;
}

[[nodiscard]] int EstimateTranscriptRows(
    const std::vector<MessageDisplayEntry>& entries, int term_cols) {
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
            content_lines = CountWrappedLines(full, term_cols);
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
                content_lines = CountWrappedLines(text, term_cols) + 3;  // label + body + margins
            } else {
                content_lines = 2;  // collapsed label + separator
            }
        } else if (entry.is_local_command_input) {
            // Local command input renders as a compact 1-line chip
            // ("/command args") — no truncation, no wrapping.
            content_lines = 1;
        } else {
            // UserTextMessage wraps at a fixed 76 cols (kPromptWrapWidth=78
            // minus 2 for the "❯ " prefix) regardless of terminal width.
            // Assistant text reserves 2 cols for the "●" bullet glyph, so
            // the body wraps at term_cols-2.  Using term_cols here would
            // underestimate on narrow terminals (e.g. 40 cols → estimate
            // 25 lines vs actual 27 for a 1000-char message), making
            // max_offset too small and preventing scroll-to-bottom.
            const int wrap_cols = (entry.role == "user")
                ? 76
                : std::max(20, term_cols - 2);
            if (entry.role == "user") {
                // UserTextMessage truncates content > 10K chars to
                // head 2500 + separator + tail 2500 before rendering.
                // Apply the same truncation here so the estimate matches
                // the actual rendered height — otherwise a 100K-char paste
                // counts 1300+ lines while rendering only ~68, creating a
                // scroll dead-zone.
                const std::string truncated =
                    ::loom::ui::messages::TruncateUserPromptText(
                        entry.content_preview);
                content_lines = CountWrappedLines(truncated, wrap_cols);
            } else if (entry.is_local_command_output ||
                       entry.is_local_jsx_output) {
                // Local command output (e.g. /help, /theme list) and JSX
                // overlay output (e.g. /skills, /agents) render every line
                // as a separate text() row — one display row per \n.  These
                // entries carry role="system" (set in app_local_command.cpp
                // and BuildVisibleMessages), so this check must precede the
                // system flat-row branch below.
                content_lines = CountWrappedLines(entry.content_preview,
                                                  wrap_cols);
            } else if (entry.role == "assistant") {
                // Assistant text renders through Markdown flexbox, which
                // wraps at word boundaries (atomic words, break at spaces)
                // with CommonMark soft/hard break semantics.  Hard character
                // wrapping underestimates for text with spaces — e.g.
                // 200 "word " at 40 cols: hard-wrap gives 27 lines,
                // word-wrap gives 29, making the last line unreachable.
                content_lines = CountWordWrappedLines(entry.content_preview,
                                                      wrap_cols);
            } else if (entry.role == "system") {
                // System messages render as flat text() rows — no Markdown
                // wrapping, always 1 line (long content overflows the
                // terminal but doesn't add vertical space).
                content_lines = 1;
            } else {
                // Other roles: hard-wrap estimate is a safe default.
                content_lines = CountWrappedLines(entry.content_preview,
                                                  wrap_cols);
            }
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

[[nodiscard]] int WheelStride(int viewport_height_lines) {
    constexpr int kMinStride = 3;
    constexpr int kMaxStride = 15;
    constexpr int kViewportDivisor = 10;  // 10% of viewport height
    const int vp = std::max(1, viewport_height_lines);
    return std::clamp(vp / kViewportDivisor, kMinStride, kMaxStride);
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

        state->messages_store.scroll_offset = target;
        const bool was_pinned = state->messages_store.scroll_pinned_to_bottom;
        state->messages_store.scroll_pinned_to_bottom = (target >= max_top);

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
    const int max_offset =
        std::max(0, EstimateTranscriptRows(visible_messages,
                     state->messages_store.viewport_width_cols) - viewport_rows);
    if (max_offset == 0) return false;

    // When pinned to bottom, scroll_offset is 0 but the view is visually
    // at max_offset (via focusPositionRelative).  Start from max_offset
    // so wheel-up actually moves the view instead of clamping 0 + (-3)
    // back to 0.
    const int base = state->messages_store.scroll_pinned_to_bottom
        ? max_offset
        : std::clamp(state->messages_store.scroll_offset, 0, max_offset);
    const int next = std::clamp(base + delta, 0, max_offset);
    state->messages_store.scroll_offset = next;
    const bool was_pinned = state->messages_store.scroll_pinned_to_bottom;
    state->messages_store.scroll_pinned_to_bottom = next >= max_offset;
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

void JumpTranscriptToBottom(ReplScreenState& state) {
    const int viewport_rows = std::max(
        1, state.messages_store.viewport_height_lines);
    const int total_lines = state.messages_store.virtual_list_active
        ? state.messages_store.virtual_jh.total()
        : EstimateTranscriptRows(BuildVisibleMessages(state),
            state.messages_store.viewport_width_cols);
    const int max_top = std::max(0, total_lines - viewport_rows);
    const int old_top = state.messages_store.scroll_pinned_to_bottom
        ? max_top
        : std::clamp(state.messages_store.scroll_offset, 0, max_top);

    state.messages_store.scroll_offset = max_top;
    if (state.messages_store.virtual_list_state) {
        namespace vl = loom::ui::messages::virtual_list;
        state.messages_store.virtual_list_state->scroll_top = max_top;
        vl::update_sticky_after_scroll(
            *state.messages_store.virtual_list_state, old_top);
    }

    state.messages_store.scroll_pinned_to_bottom = true;
    state.messages_store.pill_visible = false;
    state.messages_store.unseen_message_count = 0;
    state.messages_store.divider_index.reset();
    state.messages_store.unseen_divider.reset();
}

}  // namespace loom::ui::repl_screen
