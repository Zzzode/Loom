/// @file message_tool_result.cppm
/// @brief Tool result message rendering (success/error states)
module;

#include <cctype>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <cstddef>

export module loom.ui.messages.message_tool_result;

import std;

import loom.types.types;
// RFC 0002 F1 row 8: the ANSI/SGR -> FTXUI helpers (sgr_color_value_to_ftxui,
// apply_sgr_run, ansi_to_ftxui_elements) moved to the chrome leaf
// loom.ui.chrome.ansi_render; messages -> chrome is downward-legal.
import loom.ui.chrome.ansi_render;
import loom.ui.messages.message_components;  // for padding() Decorator
import loom.ui.visual.markdown;                     // render_markdown() for natural-language tool results

export namespace loom::ui::messages {

using namespace ftxui;

/// Unescape literal backslash escape sequences into real characters.
/// Handles JSON-style string escapes plus all practical escape levels:
///   \n    → real newline  (single-escaped)
///   \\n   → real newline  (double-escaped)
///   \\\n  → \ + newline   (literal backslash + escaped newline)
///   \r\n  → real newline  (Windows CRLF)
///   \r    → real newline  (bare CR)
///   \"    → "             (JSON-escaped quote)
///   \\    → \             (JSON-escaped backslash, when not \\n)
///   \t    → tab           (JSON-escaped tab)
///
/// Some tool results (especially from natural-language tools like
/// analyze_image) carry JSON-escaped or double-escaped newlines that
/// never got decoded, so a multi-line result renders as one huge
/// clipped line AND markdown block patterns (---, - list) fail to match.
/// JSON-escaped quotes and backslashes also leak through as literal
/// \" and \\ sequences that confuse readers.
[[nodiscard]] inline std::string unescape_literal_newlines(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        // Windows / old-Mac line endings → real newline
        if (c == '\r') {
            out += '\n';
            if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
            continue;
        }

        if (c != '\\' || i + 1 >= s.size()) {
            out += c;
            continue;
        }

        const char next = s[i + 1];

        // Check double-escaped first: \\n → newline
        // (3 bytes: backslash, backslash, n → represents literal \n in source)
        if (next == '\\' && i + 2 < s.size() && s[i + 2] == 'n') {
            out += '\n';
            i += 2;
            continue;
        }

        // Single-char escapes
        switch (next) {
            case 'n':  out += '\n'; ++i; break;   // \n → newline
            case 't':  out += '\t'; ++i; break;   // \t → tab
            case '"':  out += '"';  ++i; break;   // \" → quote
            case '\\': out += '\\'; ++i; break;   // \\ → backslash (JSON)
            case '/':  out += '/';  ++i; break;   // \/ → forward slash (JSON)
            default:   out += '\\'; break;        // unknown escape: keep backslash
        }
    }
    return out;
}

// ─── JSON text-payload unwrapper ────────────────────────────────────────
// TS REF: src/tools/MCPTool/UI.tsx tryUnwrapTextPayload (line 327)
//
// MCP tools sometimes return results as a JSON object like:
//   {"analysis":"The image shows...\\n\\n---\\n\\nDetails..."}
//   {"text":"The image shows...","confidence":"high"}
//
// TS behavior: only processes content that STARTS with '{' (JSON object),
// max 4 keys at top level, finds one "dominant" string value (>200 chars
// or contains \n and >50 chars).  Returns the dominant string for display
// via OutputLine (NOT markdown — tool output is always plain/ANSI).
//
// IMPORTANT: content like "analyze_image_result_summary: [{\"text\":\"...\"}]"
// does NOT start with '{', so this function returns nullopt and the raw
// content is displayed (matches TS screenshot behavior).
namespace detail {

/// Extract a JSON-escaped string value starting at position `pos` in `s`.
/// The opening quote has already been consumed.  Returns the unescaped
/// string and advances `pos` past the closing quote.
[[nodiscard]] std::string extract_json_string(std::string_view s, std::size_t& pos) {
    std::string result;
    bool escape = false;
    while (pos < s.size()) {
        char c = s[pos++];
        if (escape) {
            switch (c) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                default:   result += c;    break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            return result;  // closing quote found
        } else {
            result += c;
        }
    }
    return result;  // unterminated string — return what we have
}

/// TS-faithful tryUnwrapTextPayload: only matches '{' at start, max 4 keys,
/// finds dominant string value.  Returns the extracted text or nullopt.
[[nodiscard]] std::optional<std::string> try_unwrap_text_payload(std::string_view s) {
    // Trim leading whitespace
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return std::nullopt;

    // TS: only processes JSON objects starting with '{'
    // Arrays like [{"text":"..."}] or prefixes like "result: [{...}]" are
    // NOT unwrapped — raw content is shown instead.
    if (s[start] != '{') return std::nullopt;

    // Known keys that typically hold natural-language text payloads.
    // TS looks for ANY string value that is "dominant" (large enough),
    // not just specific key names — but we prioritize known text keys.
    static constexpr std::string_view kTextKeys[] = {
        "text", "messages", "content", "analysis",
        "result", "summary", "output", "description",
        "caption", "body",
    };

    // Scan for key-value patterns: "key":"value"  inside the JSON object.
    // We try each known key; if none match, we also try a generic scan for
    // any large string value (TS: "dominant string" detection).
    std::string best_value;
    for (auto key : kTextKeys) {
        std::string pat = "\"";
        pat += key;
        pat += "\":";

        std::size_t search_pos = start;
        while ((search_pos = s.find(pat, search_pos)) != std::string_view::npos) {
            std::size_t after_key = search_pos + pat.size();
            // Skip whitespace after colon
            while (after_key < s.size() && s[after_key] == ' ') ++after_key;

            // Expect a string value starting with "
            if (after_key < s.size() && s[after_key] == '"') {
                ++after_key;  // skip opening quote
                std::size_t extract_pos = after_key;
                std::string extracted = extract_json_string(s, extract_pos);

                // TS: dominant string = >200 chars OR (contains \n AND >50 chars)
                if (extracted.size() > best_value.size() &&
                    (extracted.size() > 200 ||
                     (extracted.find('\n') != std::string::npos && extracted.size() > 50))) {
                    best_value = std::move(extracted);
                }
            }
            search_pos = after_key;
        }
    }

    if (best_value.empty()) return std::nullopt;
    return best_value;
}

/// Extract text from MCP result summary format:
///   "tool_name_result_summary: [{\"text\":\"...\"}]"
///
/// Many MCP tools (especially natural-language ones like analyze_image)
/// return their results in this structured format: a descriptive prefix
/// followed by a JSON array containing one object with a "text" key.
/// The inner text is a JSON-encoded string with \n, \", \\ escapes.
///
/// TS has no explicit handler for this format (it falls through to
/// OutputLine raw display), but the TS MCP SDK's JSON-RPC parser already
/// decodes the outer transport layer, so the escapes may be partially
/// decoded before reaching the UI.  Our CPP MCP client preserves the raw
/// text, so we need this extraction step.
///
/// Returns the decoded text string, or nullopt if the pattern doesn't match.
[[nodiscard]] std::optional<std::string> try_extract_result_summary_text(
    std::string_view s) {
    // Look for "_result_summary" or "result_summary" anywhere in the string.
    // Common patterns: "analyze_image_result_summary:", "tool_result_summary:"
    auto summary_pos = s.find("result_summary");
    if (summary_pos == std::string_view::npos) return std::nullopt;

    // Find the first '[' after the summary keyword
    auto bracket_pos = s.find('[', summary_pos);
    if (bracket_pos == std::string_view::npos) return std::nullopt;

    // Find the first '{' inside the array (start of first object)
    auto brace_pos = s.find('{', bracket_pos);
    if (brace_pos == std::string_view::npos) return std::nullopt;

    // Look for "text":"..." key-value pair in the object.
    // We search for the pattern '"text":' then extract the string value.
    static constexpr std::string_view kTextKey = "\"text\":";
    auto key_pos = s.find(kTextKey, brace_pos);
    if (key_pos == std::string_view::npos) return std::nullopt;

    // Skip whitespace after the colon
    std::size_t after_key = key_pos + kTextKey.size();
    while (after_key < s.size() && s[after_key] == ' ') ++after_key;

    // Expect a string value starting with "
    if (after_key >= s.size() || s[after_key] != '"') return std::nullopt;
    ++after_key;  // skip opening quote

    // extract_json_string handles all JSON escapes (\n, \", \\, \t, etc.)
    std::size_t extract_pos = after_key;
    std::string extracted = extract_json_string(s, extract_pos);

    if (extracted.empty()) return std::nullopt;
    return extracted;
}

} // namespace detail

/// Tools whose results are LLM-generated natural language (not raw structured
/// output).  TS REF: src/components/ToolResult.tsx — AgentTool, BriefTool,
/// ExitPlanModeTool all use <Markdown> for their results.  NOTE: MCP tools
/// (like analyze_image) do NOT get markdown — they use MCPTextOutput →
/// OutputLine (plain ANSI text only).
[[nodiscard]] inline bool tool_produces_natural_language(std::string_view tool_name) {
    using namespace std::string_view_literals;
    static constexpr std::string_view kNL[] = {
        "Agent"sv,
        "Brief"sv,
        "ExitPlanMode"sv,
    };
    for (auto nl : kNL) {
        if (tool_name == nl) return true;
    }
    return false;
}

/// Tool result status
enum class ToolResultStatus {
    Success,
    Error,
    Timeout,
    Cancelled,
};

/// Tool result display options
struct ToolResultOptions {
    std::string tool_name;
    ToolResultStatus status{ToolResultStatus::Success};
    std::optional<std::string> output;
    std::optional<std::string> error_message;
    std::optional<double> duration_ms;
    bool is_truncated{false};
    bool is_transcript_mode{false};
    /// TS PARITY (2026-07-04): structured content items from MCP results.
    /// When present, the faithful renderer iterates these instead of the
    /// flattened `output` string.
    std::optional<std::vector<loom::core::ToolResultContentItem>> content_items;
};

/// Render tool result message
[[nodiscard]] inline Element render_tool_result(const ToolResultOptions& opts) {
    auto status_indicator = [&]() -> Element {
        switch (opts.status) {
            case ToolResultStatus::Success: return text("✓") | color(Color::Green);
            case ToolResultStatus::Error: return text("✗") | color(Color::Red);
            case ToolResultStatus::Timeout: return text("⏱") | color(Color::Yellow);
            case ToolResultStatus::Cancelled: return text("⊘") | color(Color::GrayDark);
        }
        return text("?");
    }();

    std::vector<Element> elements;
    elements.push_back(hbox({
        status_indicator, text(" "),
        text(opts.tool_name) | bold,
        opts.duration_ms ? (text(std::format(" ({:.0f}ms)", *opts.duration_ms)) | dim) : text(""),
    }));

    if (opts.output && !opts.output->empty()) {
        // Tool/bash output may carry embedded ANSI SGR sequences; render
        // them as colored ftxui Elements instead of leaking ESC bytes as
        // literal text.  `dim` is kept as the base style so un-styled
        // spans match the previous subdued look.  ansi_to_ftxui_elements
        // returns its per-line vector type-erased (see ansi_render.cppm);
        // cast it back and compose the lines.
        auto ansi_elems = std::static_pointer_cast<std::vector<Element>>(
            ansi_to_ftxui_elements(*opts.output));
        elements.push_back(vbox(std::move(*ansi_elems)) | dim);
    }
    if (opts.error_message) {
        elements.push_back(text(*opts.error_message) | color(Color::Red));
    }
    if (opts.is_truncated) {
        elements.push_back(text("  (output truncated)") | dim);
    }

    return vbox(elements);
}

// ─── Faithful TS renderer (UserToolResultMessage.tsx) ──────────────────────
// Mirrors the TS UserToolResultMessage dispatcher and its four sub-renderers
// (UserToolCanceledMessage, UserToolRejectMessage, UserToolErrorMessage,
// UserToolSuccessMessage) plus their shared MessageResponse envelope.
//
// DISPATCH TABLE (matches TS if-chain order):
//   1. content starts with CANCEL_MESSAGE  → Canceled  (InterruptedByUser)
//   2. content starts with REJECT_MESSAGE
//      or content == INTERRUPT_MESSAGE    → Rejected
//   3. is_error                           → Error
//   4. otherwise                           → Success
//
// Each sub-renderer falls back to the generic TS fallback component output when
// the tool-specific renderer is unavailable (tool renderers not ported yet).
//
// Visual chrome matches TS exactly:
//   • MessageResponse prefix: "  ⎿  " (dim, left-gutter style)
//   • Canceled/Rejected: single-line dim "Interrupted" / "Tool use rejected"
//   • Error: red "Error: ..." text inside MessageResponse, +N lines hint
//   • Success: tool output body (dim, ANSI-aware), no MessageResponse wrapper
//     because success output renders as part of the message body directly.

/// Kind of tool result — mirrors TS dispatch semantics.
enum class ToolResultKind {
    Success,          // normal successful tool result (default)
    Error,            // is_error = true (generic error fallback)
    Interrupted,      // INTERRUPT_MESSAGE_FOR_TOOL_USE (user Ctrl-C mid-tool)
    Rejected,         // REJECT_MESSAGE prefix (user rejected tool use)
    Canceled,         // CANCEL_MESSAGE prefix (user canceled execution)
    PlanRejected,     // PLAN_REJECTION_PREFIX (user rejected a plan)
    ClassifierDenied, // auto-mode classifier denied the tool
};

/// Faithful data model — mirrors the TS UserToolResultMessage props shape.
struct ToolResultFaithfulData {
    std::string tool_name;
    ToolResultKind kind{ToolResultKind::Success};

    /// Content field (kind-dependent):
    ///   Success        → tool output text (may contain ANSI)
    ///   Error          → error text string
    ///   PlanRejected   → plan content text
    ///   other kinds    → unused (text is fixed per kind)
    std::optional<std::string> content;

    /// TS PARITY (2026-07-04): structured content items from MCP results.
    /// When present, the Success renderer iterates these instead of using
    /// the flattened `content` string.  Each item may be "text" or "image".
    std::optional<std::vector<loom::core::ToolResultContentItem>> content_items;

    // --- Flags ---
    bool verbose{false};
    bool is_transcript_mode{false};
    bool is_truncated{false};
    std::optional<double> duration_ms;
};

namespace detail {

// ─── MessageResponse envelope helper ───────────────────────────────────────
// Mirrors TS <MessageResponse> component: a dim "  ⎿  " prefix (left gutter)
// followed by the response body.  Used for all non-success tool results.

[[nodiscard]] inline Element message_response_prefix() {
    // "  ⎿  " — two spaces + hook symbol + space = 5 columns total,
    // matching TS {"  "}⎿&nbsp; rendered width.
    return text("  \xe2\x8e\xbf  ") | dim;
}

/// Wrap content in a MessageResponse-style envelope.
[[nodiscard]] inline Element wrap_message_response(Element body) {
    return hbox({ message_response_prefix(), std::move(body) });
}

// ─── InterruptedByUser.tsx ─────────────────────────────────────────────────
// "Interrupted · What should Loom do instead?"  (dim text, middot separator)

[[nodiscard]] inline Element render_interrupted_by_user() {
    return hbox({
        text("Interrupted ") | dim,
        text("\xc2\xb7 What should Loom do instead?") | dim,
    });
}

// ─── FallbackToolUseRejectedMessage.tsx ───────────────────────────────────
// MessageResponse + InterruptedByUser (single-line height=1).

[[nodiscard]] inline Element render_fallback_rejected() {
    return wrap_message_response(render_interrupted_by_user());
}

// ─── RejectedToolUseMessage.tsx ───────────────────────────────────────────
// "Tool use rejected" (dim, single line inside MessageResponse).

[[nodiscard]] inline Element render_rejected_tool_use() {
    return wrap_message_response(text("Tool use rejected") | dim);
}

// ─── Fallback error helpers ───────────────────────────────────────────────
// Mirrors FallbackToolUseErrorMessage.tsx error-processing pipeline:
//   1. extractTag(result, "tool_use_error") ?? result
//   2. removeSandboxViolationTags
//   3. strip <error> tags (keep content)
//   4. trim
//   5. if !verbose && includes "InputValidationError: " → "Invalid tool parameters"
//   6. else if startsWith "Error: " or "Cancelled: " → keep
//   7. else → "Error: " + trimmed

/// Extract inner content of the first <tag>...</tag> pair.
/// If the tag is not found, returns the full input.
[[nodiscard]] inline std::string extract_tag(
    std::string_view text, std::string_view tag) {
    std::string open;
    open.reserve(tag.size() + 2);
    open += '<';
    open.append(tag.data(), tag.size());
    open += '>';
    std::string close;
    close.reserve(tag.size() + 3);
    close += "</";
    close.append(tag.data(), tag.size());
    close += '>';

    auto start = text.find(open);
    if (start == std::string_view::npos) return std::string(text);
    start += open.size();

    auto end = text.find(close, start);
    if (end == std::string_view::npos)
        return std::string(text.substr(start));
    return std::string(text.substr(start, end - start));
}

/// Remove <sandbox_violation>...</sandbox_violation> tag pairs (keep content).
/// Mirrors TS removeSandboxViolationTags.
[[nodiscard]] inline std::string remove_sandbox_tags(std::string_view text) {
    constexpr std::string_view kOpen  = "<sandbox_violation>";
    constexpr std::string_view kClose = "</sandbox_violation>";

    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;

    while (pos < text.size()) {
        auto open = text.find(kOpen, pos);
        if (open == std::string_view::npos) {
            result.append(text.substr(pos));
            break;
        }
        result.append(text.substr(pos, open - pos));
        pos = open + kOpen.size();

        auto close = text.find(kClose, pos);
        if (close == std::string_view::npos) {
            result.append(text.substr(pos));
            break;
        }
        result.append(text.substr(pos, close - pos));
        pos = close + kClose.size();
    }
    return result;
}

/// Strip <error> and </error> tags (keep inner content).
/// Mirrors TS .replace(/<\/?error>/g, "").
[[nodiscard]] inline std::string strip_error_tags(std::string_view text) {
    constexpr std::string_view kOpen  = "<error>";
    constexpr std::string_view kClose = "</error>";

    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;

    while (pos < text.size()) {
        auto lt = text.find('<', pos);
        if (lt == std::string_view::npos) {
            result.append(text.substr(pos));
            break;
        }
        if (text.substr(lt, kOpen.size()) == kOpen) {
            result.append(text.substr(pos, lt - pos));
            pos = lt + kOpen.size();
        } else if (text.substr(lt, kClose.size()) == kClose) {
            result.append(text.substr(pos, lt - pos));
            pos = lt + kClose.size();
        } else {
            result += text[lt];
            pos = lt + 1;
        }
    }
    return result;
}

/// Trim leading and trailing whitespace from a string (in-place).
inline void trim_string(std::string& s) {
    // ltrim
    auto it = s.begin();
    while (it != s.end() &&
           std::isspace(static_cast<unsigned char>(*it))) {
        ++it;
    }
    s.erase(s.begin(), it);
    // rtrim
    auto rit = s.end();
    while (rit != s.begin() &&
           std::isspace(static_cast<unsigned char>(*(rit - 1)))) {
        --rit;
    }
    s.erase(rit, s.end());
}

constexpr int kMaxRenderedLines = 10;

[[nodiscard]] inline int count_newlines(std::string_view s) {
    int count = 0;
    for (char c : s) if (c == '\n') ++count;
    return count;
}

/// Take the first n lines of s, returned as a string without the trailing
/// newline of the n-th line (if it ends with one).
[[nodiscard]] inline std::string take_first_lines(std::string_view s, int n) {
    if (n <= 0) return "";
    std::size_t pos = 0;
    int lines = 0;
    while (pos < s.size() && lines < n) {
        auto nl = s.find('\n', pos);
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
        ++lines;
    }
    if (lines == n && pos > 0) --pos; // back up over the final \n
    return std::string(s.substr(0, pos));
}

/// Render the fallback error message (FallbackToolUseErrorMessage.tsx).
[[nodiscard]] inline Element render_fallback_error(
    std::string_view error_text, bool verbose) {
    std::string extracted = extract_tag(error_text, "tool_use_error");
    std::string without_sandbox = remove_sandbox_tags(extracted);
    std::string without_error = strip_error_tags(without_sandbox);
    trim_string(without_error);

    std::string error;
    if (!verbose &&
        without_error.find("InputValidationError: ") != std::string::npos) {
        error = "Invalid tool parameters";
    } else if (without_error.substr(0, 7) == "Error: " ||
               without_error.substr(0, 11) == "Cancelled: ") {
        error = without_error;
    } else {
        error.reserve(without_error.size() + 7);
        error = "Error: ";
        error += without_error;
    }

    int total_lines = count_newlines(error) + 1;
    int plus_lines = total_lines - kMaxRenderedLines;

    std::string display_text;
    if (verbose || plus_lines <= 0) {
        display_text = error;
    } else {
        display_text = take_first_lines(error, kMaxRenderedLines);
    }

    Elements lines;
    {
        auto ansi_elems = std::static_pointer_cast<std::vector<Element>>(
            ansi_to_ftxui_elements(display_text));
        lines.push_back(vbox(std::move(*ansi_elems)) | color(Color::Red));
    }

    if (!verbose && plus_lines > 0) {
        std::string hint;
        hint.reserve(32);
        hint = "\xe2\x80\xa6 +";
        hint += std::to_string(plus_lines);
        hint += " line";
        if (plus_lines != 1) hint += 's';
        hint += " (ctrl+o to see all)";
        lines.push_back(text(hint) | dim);
    }

    Element body = vbox(std::move(lines));
    return wrap_message_response(std::move(body));
}

// ─── RejectedPlanMessage.tsx ──────────────────────────────────────────────
// "User rejected Loom's plan:" label + plan content in a round-border box
// with planMode color (purple).

[[nodiscard]] inline Element render_rejected_plan(
    std::string_view plan_content) {
    // TS renders plan as Markdown inside a round-border box.  Markdown for
    // plan content isn't ported yet, so we render plain text inside a
    // ROUNDED-border box with plan-mode color.  padding(0,1,0,1) gives 1
    // cell of horizontal padding (left+right) matching TS paddingX={1}.
    //
    // Split plan content on newlines so multi-line plans render correctly.
    Elements plan_lines;
    std::size_t start = 0;
    while (start <= plan_content.size()) {
        auto nl = plan_content.find('\n', start);
        if (nl == std::string_view::npos) {
            plan_lines.push_back(text(
                std::string(plan_content.substr(start))));
            break;
        }
        plan_lines.push_back(text(
            std::string(plan_content.substr(start, nl - start))));
        start = nl + 1;
    }

    Element plan_box = vbox(std::move(plan_lines))
        | borderStyled(BorderStyle::ROUNDED, Color::Purple)
        | padding(/*top=*/0, /*right=*/1, /*bottom=*/0, /*left=*/1);

    Elements inner;
    inner.push_back(
        text("User rejected Loom's plan:") | color(Color::GrayLight));
    inner.push_back(std::move(plan_box));

    return wrap_message_response(vbox(std::move(inner)));
}

} // namespace detail

// ─── Faithful top-level dispatcher ────────────────────────────────────────
/// Matches UserToolResultMessage.tsx if-chain exactly:
///   Canceled → Rejected → Error → Success
///
/// Each branch renders the same visual chrome as its TS counterpart using
/// the fallback components (tool-specific renderers are not yet ported).
///
/// `add_margin` (TS REF: UserToolResultMessage → MessageRow marginTop=1)
/// controls the blank separator line above tool result rows.  TS always
/// gives tool results marginTop=1 (they never have metadata), so this
/// defaults true; the caller threads the turn-boundary-computed value.
[[nodiscard]] inline Element RenderToolResultMessageFaithful(
    const ToolResultFaithfulData& data, bool add_margin = true) {
    using K = ToolResultKind;
    Element result;

    switch (data.kind) {
        case K::Canceled:
            // UserToolCanceledMessage → MessageResponse + InterruptedByUser
            result = detail::wrap_message_response(
                detail::render_interrupted_by_user());
            break;

        case K::Interrupted:
            // UserToolErrorMessage: includes INTERRUPT_MESSAGE
            // → MessageResponse + InterruptedByUser
            result = detail::wrap_message_response(
                detail::render_interrupted_by_user());
            break;

        case K::Rejected:
            // UserToolRejectMessage → fallback path
            result = detail::render_fallback_rejected();
            break;

        case K::PlanRejected:
            // UserToolErrorMessage: PLAN_REJECTION_PREFIX
            result = detail::render_rejected_plan(
                data.content.value_or(""));
            break;

        case K::ClassifierDenied: {
            // UserToolErrorMessage: isClassifierDenial
            std::string msg =
                "Denied by auto mode classifier \xc2\xb7 /feedback if incorrect";
            result = detail::wrap_message_response(text(msg) | dim);
            break;
        }

        case K::Error: {
            // UserToolErrorMessage fallback path
            std::string content = data.content.value_or("Tool execution failed");
            result = detail::render_fallback_error(content, data.verbose);
            break;
        }

        case K::Success: {
            // TS: UserToolSuccessMessage renders tool.renderToolResultMessage()
            // directly — NO "✓ tool_name" header.  The result body is wrapped in
            // <MessageResponse> (⎿ connector) by the tool's own renderer.
            // For BashTool: stdout wrapped in MessageResponse.

            bool has_content_items = data.content_items && !data.content_items->empty();
            bool has_flat_content = data.content && !data.content->empty();

            if (!has_content_items && !has_flat_content) {
                // Empty success: "(No output)" in MessageResponse style
                result = detail::wrap_message_response(
                    text("(No output)") | dim);
                break;
            }

            Elements elems;

            if (has_content_items) {
                for (const auto& item : *data.content_items) {
                    if (item.type == "text") {
                        std::string text_content = item.text;
                        if (auto unwrapped = detail::try_unwrap_text_payload(text_content)) {
                            text_content = std::move(*unwrapped);
                        }
                        // Trim trailing newlines — shell output typically ends
                        // with \n which would produce an extra blank ⎿ line.
                        while (!text_content.empty() &&
                               (text_content.back() == '\n' || text_content.back() == '\r'))
                            text_content.pop_back();
                        if (text_content.empty()) continue;
                        auto ansi_elems = std::static_pointer_cast<std::vector<Element>>(
                            ansi_to_ftxui_elements(text_content));
                        elems.push_back(detail::wrap_message_response(
                            vbox(std::move(*ansi_elems))));
                    } else if (item.type == "image") {
                        elems.push_back(detail::wrap_message_response(
                            text("[Image]") | dim));
                    }
                }
            } else {
                std::string output = *data.content;
                if (auto unwrapped = detail::try_unwrap_text_payload(output)) {
                    output = std::move(*unwrapped);
                }
                constexpr std::size_t kMaxLen = 4000;
                if (output.size() > kMaxLen) {
                    output = output.substr(0, kMaxLen) + "\xE2\x80\xA6";
                }
                // Render each line with ⎿ prefix (MessageResponse style)
                std::size_t line_start = 0;
                while (line_start < output.size()) {
                    auto nl = output.find('\n', line_start);
                    std::string_view line = (nl == std::string::npos)
                        ? std::string_view(output).substr(line_start)
                        : std::string_view(output).substr(line_start, nl - line_start);
                    auto ansi_elems = std::static_pointer_cast<std::vector<Element>>(
                        ansi_to_ftxui_elements(line));
                    elems.push_back(detail::wrap_message_response(
                        vbox(std::move(*ansi_elems))));
                    if (nl == std::string::npos) break;
                    line_start = nl + 1;
                }
            }

            if (data.is_truncated) {
                elems.push_back(detail::wrap_message_response(
                    text("(output truncated)") | dim));
            }

            result = vbox(std::move(elems));
            break;
        }
    }

    if (!result) result = text("");
    if (add_margin) {
        return vbox({text(""), std::move(result)});
    }
    return result;
}

} // namespace loom::ui::messages
