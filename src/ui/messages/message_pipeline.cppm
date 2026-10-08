/// @file message_pipeline.cppm
/// @brief Faithful message-pipeline helpers.  Transforms raw engine
///        StreamEvents into render-ready shapes via pure helper functions.
///
/// Pipeline stages:
///   Stage 1 EVENT_DEDUP        → dedup per-block-index event lifecycle
///   Stage 2 STREAM_NORMALIZE   → onEvent() delta flattening
///   Stage 3 CONTENT_BLOCK_MERGE → buildTurns()
///   Stage 4 USER_INPUT_FILTER  → extractTags()
///   Stage 5 TOOL_RESULT_AUGMENT→ augmentToolResult()
///
/// CPP mapping:
///   Stages 2 and 3 are driven by app.cppm's on_event lambda; this module
///   provides pure helpers for Stages 1/4/5 plus a PipelineState struct
///   that app.cppm instantiates once per query thread.
///
/// WHY A SEPARATE MODULE (P0-2 BLOCKER justifications):
///   * Stage 1 (dedup): Reconnects or retransmits can cause duplicate events
///     for the same ContentBlock index, producing duplicate tool rows or
///     spurious "complete" transitions mid-stream.  Without it the transcript
///     visually flickers on flaky connections.
///   * Stage 4 (tag filter): <bash-input>/<quoted-reply>/<tool:*> tags must be
///     stripped from display text BEFORE landing in the user transcript;
///     otherwise the user sees raw XML.  Today user_bash_input_message.cppm
///     does the strip locally but user_text_message.cppm has no equivalent.
///   * Stage 5 (tool augment): QueryEngine::stream_tool returns just `result`
///     + `is_error`; the pipeline additionally surfaces `truncated: bool`,
///     `error_code`, and a short `preview: string` used by the compact
///     tool-use card.
// ────────────────────────────────────────────────────────────────────────
module;

#include <cctype>
#include <cstddef>
#include <cstdint>

#include <ftxui/dom/elements.hpp>

export module loom.ui.messages.message_pipeline;

import std;

import loom.ui.foundation.design_figures;
import loom.ui.messages.xml_tags;

export namespace loom::ui::messages::pipeline {

using namespace ftxui;
namespace figs = loom::ui::design::figures;

// ─── Forward declarations for downstream render sites ──────────────────────
// We don't import heavy render modules here (would create BMI circular deps),
// instead we pass handle types that the render layer registers at startup.

/// Opaque tag describing the CLASS of structural row emitted by Stage 4
/// (user-input filter).  Render sites switch on this to pick the right
/// component — the render-layer dispatcher.
enum class UserRowKind : std::uint8_t {
    kPlainText = 0,           // literal user text, no tags
    kBashInput = 1,           // <bash-input> wrapped text → RenderUserBashInput
    kQuotedReply = 2,         // <quoted-reply> + body (two rows)
    kToolInvocation = 3,      // <tool:name>result</tool:name> block
    kSystemCommand = 4,       // <system> wrapper (stripped body)
    kAttachment = 5,          // <at-file> / <at-agent> / <at-tool>
};

/// Output of Stage 4 USER_INPUT_FILTER: a single logical user row stripped
/// of its XML envelope, classified for the right render component.
struct FilteredUserRow {
    UserRowKind kind = UserRowKind::kPlainText;
    std::string display_text;          // what to show (no XML tags)
    std::string quoted_reply;          // if kind == kQuotedReply: the quoted block
    std::string attachment_ref;        // if kind == kAttachment: raw "@ref"
    std::string tool_name;             // if kind == kToolInvocation: the "name" part
    bool is_transcript_only = false;   // showInTranscript override
};

// ─── Stage 1 Dedup: DedupTracker ───────────────────────────────────────────
/// Tracks per-index event lifecycle transitions (Start → Delta* → Stop) for
/// each ContentBlock index, plus per tool_use_id for execution events.
///
/// CORE INVARIANT: A given `index: size_t` transitions exactly once:
///   NotSeen ──Start──► Open ──Delta*──► Stopped (terminal)
///
/// A duplicate Start (e.g. reconnect replay) for an already-Stopped index
/// is SILENTLY DROPPED (returns false from should_accept).  A duplicate Stop
/// is also dropped (prevents the streaming_tools[id].complete flag from
/// bouncing true→false→true when the server replays events).
class DedupTracker {
  public:
    enum class IndexState : std::uint8_t {
        kNotSeen = 0,
        kOpen    = 1,    // Start seen, any number of Deltas allowed
        kStopped = 2,    // Stop seen — further events rejected
    };

    /// Reset the tracker at the beginning of each new query / turn.
    void clear() noexcept {
        index_states_.clear();
        seen_tool_use_ids_.clear();
    }

    /// QueryEngine::ContentBlockStart(index).  Returns true iff the caller
    /// should apply the event; false means it's a dedup drop.
    [[nodiscard]] bool should_accept_start(std::size_t index) {
        auto it = index_states_.find(index);
        if (it == index_states_.end()) {
            index_states_.emplace(index, IndexState::kOpen);
            return true;
        }
        if (it->second == IndexState::kStopped) {
            // Reconnect replay for an already-finished block.  Drop.
            return false;
        }
        // Already open: a second Start is bogus but harmless — keep state.
        return true;
    }

    /// QueryEngine::ContentBlockDelta(index).  Accept as long as not Stopped.
    /// (We don't bother to individually dedup Deltas because the server
    /// monotonically appends to them; if we did, we'd need a byte-hash per
    /// delta which is not worth the footprint.)
    [[nodiscard]] bool should_accept_delta(std::size_t index) {
        auto it = index_states_.find(index);
        if (it == index_states_.end()) {
            // Out-of-order: Delta before Start.  Rare, but accept anyway
            // (promote to Open; Start will land later and dedup correctly
            //  because kOpen is non-terminal).
            index_states_.emplace(index, IndexState::kOpen);
            return true;
        }
        return it->second != IndexState::kStopped;
    }

    /// QueryEngine::ContentBlockStop(index).  Idempotent — first call wins.
    [[nodiscard]] bool should_accept_stop(std::size_t index) {
        auto it = index_states_.find(index);
        if (it == index_states_.end()) {
            index_states_.emplace(index, IndexState::kStopped);
            return true;
        }
        if (it->second == IndexState::kStopped) return false;  // dedup!
        it->second = IndexState::kStopped;
        return true;
    }

    /// ToolExecutionStart / Progress / End use tool_use_id keys.  Return true
    /// when the execution event is novel.  Progress events are always
    /// accepted; only the Start/End pair is one-shot.
    [[nodiscard]] bool should_accept_exec_start(std::string_view tool_use_id) {
        // A tool that has already been `End`ed cannot restart its Start.
        const auto [it, inserted] = seen_tool_use_ids_.emplace(std::string(tool_use_id));
        (void)it;
        return inserted;   // first-ever mention → accept.
    }

    [[nodiscard]] bool should_accept_exec_end(std::string_view tool_use_id) {
        // Accept on first End call per id; subsequent calls are dedup.
        auto key = std::string("end:") + std::string(tool_use_id);
        const auto [it, inserted] = seen_tool_use_ids_.emplace(std::move(key));
        (void)it;
        return inserted;
    }

    /// Reset per-index dedup state (for a new API call within the same query
    /// tool-loop).  Preserves per-tool-use-id state so tool execution events
    /// that span the boundary aren't double-processed.
    void clear_indices() noexcept {
        index_states_.clear();
    }

    /// Expose state for tests.
    [[nodiscard]] IndexState state_of(std::size_t idx) const {
        auto it = index_states_.find(idx);
        return (it == index_states_.end()) ? IndexState::kNotSeen : it->second;
    }

  private:
    // NOTE: std::unordered_map over a flat vector for generality; typical
    // turn cardinality is < 30 indices, so either data structure is fine.
    std::unordered_map<std::size_t, IndexState> index_states_;
    // Serves double duty: raw id → "seen any Start", "end:<id>" → "seen End".
    std::set<std::string> seen_tool_use_ids_;
};

// ─── Stage 4 USER_INPUT_FILTER: ExtractTag + classify ───────────────────────
//
// PRINCIPLE: extractTags runs BEFORE a user utterance is
// appended to the conversation state, so the transcript never sees the raw
// XML wrappers.  The `<bash-input>` tag specifically is added by the shell
// input mode (not typed by the user) and must be peeled off before display.
//
// SUPPORTED TAGS (full set, faithful):
//   <bash-input>   INPUT   </bash-input>    → kind kBashInput
//   <quoted-reply> QUOTE   </quoted-reply>  → kind kQuotedReply (quote + rest)
//   <tool:NAME>    RESULT  </tool:NAME>     → kind kToolInvocation
//   <system>       TXT     </system>        → kind kSystemCommand
//   <at-file>PATH</at-file>                 → kind kAttachment "@PATH"
//   <at-agent>NAME</at-agent>               → kind kAttachment "@NAME"
//   <at-tool>NAME</at-tool>                 → kind kAttachment "@tool:NAME"
//   plain text with no tags                 → kind kPlainText
//
// Nested tags are flattened left-to-right.

using xml_tags::extract_tag;

/// Strip ALL instances of `<tag_name>…</tag_name>` from `text`, in place.
/// Used to peel off processed envelope tags so downstream tags match cleanly.
inline void strip_tag_all(std::string& text, std::string_view tag_name) noexcept {
    if (tag_name.empty()) return;
    const std::string open  = std::string("<") + std::string(tag_name) + ">";
    const std::string close = std::string("</") + std::string(tag_name) + ">";

    std::string out;
    out.reserve(text.size());
    std::string_view view(text);
    while (!view.empty()) {
        const auto s = view.find(open);
        if (s == std::string_view::npos) {
            out.append(view);
            break;
        }
        out.append(view.substr(0, s));
        const auto content_start = s + open.size();
        const auto e = view.find(close, content_start);
        if (e == std::string_view::npos) {
            // Unterminated open tag — keep trailing content.
            out.append(view.substr(content_start));
            break;
        }
        view = view.substr(e + close.size());
    }
    text = std::move(out);
}

/// Dynamic tag match: given `<tool:NAME>`, match any `<tool:*>` prefix and
/// return the extracted tool name.  std::nullopt if not present.
[[nodiscard]] inline std::optional<std::pair<std::string /*name*/, std::string /*content*/>>
match_tool_tag(std::string_view text) noexcept {
    const auto s = text.find("<tool:");
    if (s == std::string_view::npos) return std::nullopt;
    const auto name_start = s + 6;   // len("<tool:")
    const auto name_end = text.find('>', name_start);
    if (name_end == std::string_view::npos) return std::nullopt;
    const std::string name(text.substr(name_start, name_end - name_start));
    const std::string close = "</tool:" + name + ">";
    const auto content_start = name_end + 1;
    const auto e = text.find(close, content_start);
    if (e == std::string_view::npos) {
        return std::make_pair(name, std::string(text.substr(content_start)));
    }
    return std::make_pair(
        name,
        std::string(text.substr(content_start, e - content_start)));
}

/// Stage 4 ENTRY POINT: take raw user input (possibly with XML tags) and
/// split into a list of classified display rows.
///
/// Typical input → output examples:
///   "!ls -la"                          → [{kPlainText, "!ls -la"}]
///   "<bash-input>make clean</bash-input>"
///                                      → [{kBashInput, "make clean"}]
///   "<quoted-reply># old issue</quoted-reply>new comment"
///                                      → [{kQuotedReply, "new comment", "# old issue"}]
[[nodiscard]] inline std::vector<FilteredUserRow> filter_user_text(
    std::string_view raw_text) noexcept
{
    std::vector<FilteredUserRow> rows;
    // Fast path — no angle brackets → single plain-text row.
    if (raw_text.find('<') == std::string_view::npos) {
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kPlainText,
            .display_text = std::string(raw_text),
            .quoted_reply = {},
            .attachment_ref = {},
            .tool_name = {},
            .is_transcript_only = false,
        });
        return rows;
    }

    std::string working(raw_text);

    // 1) <bash-input>
    if (auto bash = extract_tag(working, "bash-input")) {
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kBashInput,
            .display_text = std::move(*bash),
            .quoted_reply = {},
            .attachment_ref = {},
            .tool_name = {},
            .is_transcript_only = false,
        });
        strip_tag_all(working, "bash-input");
    }

    // 2) <quoted-reply> — peel out and keep remainder as display_text
    if (auto quote = extract_tag(working, "quoted-reply")) {
        std::string remainder = working;
        strip_tag_all(remainder, "quoted-reply");
        // Trim leading/trailing whitespace of remainder.
        while (!remainder.empty() && std::isspace(static_cast<unsigned char>(remainder.front())))
            remainder.erase(remainder.begin());
        while (!remainder.empty() && std::isspace(static_cast<unsigned char>(remainder.back())))
            remainder.pop_back();
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kQuotedReply,
            .display_text = std::move(remainder),
            .quoted_reply = std::move(*quote),
            .attachment_ref = {},
            .tool_name = {},
            .is_transcript_only = false,
        });
        return rows;   // <quoted-reply> is the outermost classifying tag
    }

    // 3) <tool:NAME> … </tool:NAME> — can appear multiple times, once per tool
    //    block in a user-pasted message.  One per utterance; we
    //    support many for robustness.
    for (;;) {
        auto tool_match = match_tool_tag(working);
        if (!tool_match) break;
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kToolInvocation,
            .display_text = std::move(tool_match->second),
            .quoted_reply = {},
            .attachment_ref = {},
            .tool_name = std::move(tool_match->first),
            .is_transcript_only = false,
        });
        // strip it by name
        strip_tag_all(working, "tool:" + rows.back().tool_name);
    }

    // 4) <system> stripped wrapper
    if (auto sys = extract_tag(working, "system")) {
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kSystemCommand,
            .display_text = std::move(*sys),
            .quoted_reply = {},
            .attachment_ref = {},
            .tool_name = {},
            .is_transcript_only = false,
        });
        strip_tag_all(working, "system");
    }

    // 5) <at-file> / <at-agent> / <at-tool> attachments
    for (const char* tag : {"at-file", "at-agent", "at-tool"}) {
        for (;;) {
            auto val = extract_tag(working, tag);
            if (!val) break;
            std::string prefix;
            if (tag[3] == 'f') prefix = "@";                   // at-file → @
            else if (tag[3] == 'a') prefix = "@";               // at-agent → @
            else prefix = "@tool:";                             // at-tool → @tool:
            rows.push_back(FilteredUserRow{
                .kind = UserRowKind::kAttachment,
                .display_text = {},
                .quoted_reply = {},
                .attachment_ref = prefix + std::move(*val),
                .tool_name = {},
                .is_transcript_only = false,
            });
            strip_tag_all(working, tag);
        }
    }

    // 6) Emit remaining plain text (if any non-whitespace content remains)
    {
        std::string_view v(working);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.remove_prefix(1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))  v.remove_suffix(1);
        if (!v.empty()) {
            rows.push_back(FilteredUserRow{
                .kind = UserRowKind::kPlainText,
                .display_text = std::string(v),
                .quoted_reply = {},
                .attachment_ref = {},
                .tool_name = {},
                .is_transcript_only = false,
            });
        }
    }

    if (rows.empty()) {
        // Empty-or-whitespace-only user input.  Emit a plain placeholder so
        // the render layer never has to size-zero-list branch.
        rows.push_back(FilteredUserRow{
            .kind = UserRowKind::kPlainText,
            .display_text = std::string(raw_text),
            .quoted_reply = {},
            .attachment_ref = {},
            .tool_name = {},
            .is_transcript_only = false,
        });
    }
    return rows;
}

// ─── Stage 5 TOOL_RESULT_AUGMENT ────────────────────────────────────────────
//
// Augment fields beyond what QueryEngine returns:
//   * truncated   : bool   (result bytes > kMaxToolPreviewBytes)
//   * error_code  : int    (parsed from "Error 123:" or exit-code prefix)
//   * preview     : string (first 200 chars / first non-empty line, collapse WS)
//
// NOTE: These are display-only fields; the engine preserves the FULL result
// text in its conversation memory regardless of truncation here.

inline constexpr std::size_t kMaxToolPreviewBytes = 4096;   // 4 * 1024
inline constexpr std::size_t kMaxCompactPreviewChars = 200; // compactCard preview

struct AugmentedToolResult {
    bool        truncated   = false;
    int         error_code  = 0;        // 0 = no code, >0 = shell exit/HTTP code
    std::string preview;                // compact one-liner
};

/// Extract a numeric error code from the beginning of a tool result.
/// Recognised patterns:
///   "Error 42: ..."            → 42
///   "Exit code: 127"           → 127
///   "[exit_code=1]"            → 1
///   "HTTP 404 Not Found"       → 404
[[nodiscard]] inline int parse_error_code(std::string_view result) noexcept {
    if (result.empty()) return 0;

    // Lowercase the first ~80 chars for pattern matching (in-place, ASCII-only).
    char head[96] = {0};
    const std::size_t n = std::min<std::size_t>(sizeof(head) - 1, result.size());
    for (std::size_t i = 0; i < n; ++i) {
        char c = result[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        head[i] = c;
    }
    std::string_view s(head, n);

    // Pattern 1: "error <digits>"
    if (auto p = s.find("error "); p != std::string_view::npos) {
        auto q = p + 6;
        int val = 0;
        while (q < n && std::isdigit(static_cast<unsigned char>(head[q]))) {
            val = val * 10 + (head[q] - '0');
            ++q;
        }
        if (val > 0) return val;
    }
    // Pattern 2: "exit code: <digits>" or "exit_code"
    if (auto p = s.find("exit"); p != std::string_view::npos) {
        // Walk to first digit after "exit"
        auto q = p + 4;
        while (q < n && !std::isdigit(static_cast<unsigned char>(head[q]))) ++q;
        int val = 0;
        while (q < n && std::isdigit(static_cast<unsigned char>(head[q]))) {
            val = val * 10 + (head[q] - '0');
            ++q;
        }
        if (val > 0) return val;
    }
    // Pattern 3: "http <digits>"
    if (auto p = s.find("http "); p != std::string_view::npos) {
        auto q = p + 5;
        int val = 0;
        while (q < n && std::isdigit(static_cast<unsigned char>(head[q]))) {
            val = val * 10 + (head[q] - '0');
            ++q;
        }
        // HTTP codes are >= 100
        if (val >= 100) return val;
    }
    return 0;
}

/// Build the compact one-line preview used on collapsed tool cards.
/// compactPreview = take(firstNonEmptyLine, 200 chars)
[[nodiscard]] inline std::string build_compact_preview(std::string_view result) noexcept {
    if (result.empty()) return {};
    std::string_view v = result;
    // Skip leading pure-whitespace lines.
    while (!v.empty()) {
        const auto nl = v.find_first_of("\r\n");
        std::string_view line = (nl == std::string_view::npos)
            ? v : v.substr(0, nl);
        std::string_view t = line;
        while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front()))) t.remove_prefix(1);
        while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back())))  t.remove_suffix(1);
        if (!t.empty()) {
            // First non-empty line — truncate to kMaxCompactPreviewChars codepoints.
            // We count UTF-8 by start bytes to avoid slicing a multi-byte sequence.
            std::size_t cps = 0;
            std::size_t bytes = 0;
            const std::size_t limit = kMaxCompactPreviewChars;
            while (bytes < t.size() && cps < limit) {
                unsigned char c = static_cast<unsigned char>(t[bytes]);
                // Count start bytes: not 0x80..0xBF
                if (c < 0x80 || c >= 0xC0) ++cps;
                ++bytes;
            }
            std::string out(t.substr(0, bytes));
            if (t.size() > bytes) {
                out.append(figs::kEllipsis);
            }
            return out;
        }
        if (nl == std::string_view::npos) break;
        v = v.substr(nl + 1);
    }
    return {};
}

[[nodiscard]] inline AugmentedToolResult augment_tool_result(
    std::string_view full_result, bool is_error) noexcept
{
    AugmentedToolResult a;
    a.truncated  = full_result.size() > kMaxToolPreviewBytes;
    a.error_code = is_error ? parse_error_code(full_result) : 0;
    a.preview    = build_compact_preview(full_result);
    return a;
}

}  // namespace loom::ui::messages::pipeline
