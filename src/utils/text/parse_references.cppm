// @file parse_references.cppm
// @brief Inline-[Image #N] / [Pasted text #N +K lines] / [...Truncated text #N]
// placeholder parsing and formatting.
module;

#include <cstddef>

export module loom.text.parse_references;

import std;

export namespace loom::utils {

/// A single placeholder match. `index` is a UTF-8 byte offset.
struct ReferenceMatch {
    int id;
    std::string match;
    std::size_t index;
};

[[nodiscard]] inline std::string format_image_ref(int id) {
    return std::format("[Image #{}]", id);
}

/// numLines == 0 ⇒ no "+N lines" suffix (newline count, not line count).
[[nodiscard]] inline std::string format_pasted_text_ref(int id, int num_lines) {
    if (num_lines <= 0) return std::format("[Pasted text #{}]", id);
    return std::format("[Pasted text #{} +{} lines]", id, num_lines);
}

/// Produces "[...Truncated text #N +M lines...]" — the placeholder inserted
/// between head(500) and tail(500) when a >10K char paste is truncated.
[[nodiscard]] inline std::string format_truncated_text_ref(int id, int num_lines) {
    return std::format("[...Truncated text #{} +{} lines...]", id, num_lines);
}

/// Counts newline matches using /\r\n|\r|\n/g semantics — equivalent to the
/// number of line-BREAK sequences in the text.  For "a\nb\nc" this returns 2
/// (NOT 3), following the "newline count, not line count" convention.
///
/// Algorithm: walk the string once, counting each \r\n pair as 1 break, each
/// standalone \r or \n as 1 break.
[[nodiscard]] inline int get_pasted_text_ref_num_lines(std::string_view text) {
    int count = 0;
    const std::size_t n = text.size();
    for (std::size_t i = 0; i < n; ++i) {
        char c = text[i];
        if (c == '\r') {
            ++count;
            if (i + 1 < n && text[i + 1] == '\n') {
                ++i;  // \r\n counted as one break
            }
        } else if (c == '\n') {
            ++count;
        }
    }
    return count;
}

/// Pattern (ECMAScript, g):
///   /\[(Pasted text|Image|\.\.\.Truncated text) #(\d+)(?: \+\d+ lines)?(\.)*\]/g
/// Returns matches in increasing `index` order; entries with `id <= 0` are
/// dropped.
[[nodiscard]] inline std::vector<ReferenceMatch> parse_references(std::string_view input) {
    // std::basic_regex has no string_view ctor — materialize once.
    const std::string s(input);
    // NOTE: ECMAScript mode is the default; std::regex ECMAScript grammar
    // supports exactly the features used in the pattern (alternation,
    // capturing groups, non-capturing groups, ?, *, +, \d, character class
    // escapes on literals).
    static const std::regex pattern(
        R"(\[(Pasted text|Image|\.\.\.Truncated text) #(\d+)(?: \+\d+ lines)?(\.)*\])",
        std::regex::ECMAScript);

    std::vector<ReferenceMatch> out;
    auto begin = std::sregex_iterator(s.begin(), s.end(), pattern);
    auto end = std::sregex_iterator();
    out.reserve(4);
    for (auto it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        // m[2] = digits capture
        int id = 0;
        try {
            id = std::stoi(m[2].str());
        } catch (...) {
            // Out of range or empty — skip.
            id = 0;
        }
        if (id <= 0) continue;
        out.push_back(ReferenceMatch{
            .id = id,
            .match = m[0].str(),
            .index = static_cast<std::size_t>(m.position(0)),
        });
    }
    return out;
}

/// If `text.size() > 10000`, truncates to head(500) + [...Truncated text #N +M lines...] +
/// tail(500).  Returns the display text and the truncated middle content (empty if
/// no truncation was applied).  `paste_id` is used in the placeholder ref.
///
/// Constants:
///   TRUNCATION_THRESHOLD = 10000  (chars before truncation kicks in)
///   PREVIEW_LENGTH       = 1000   (total chars preserved: 500 head + 500 tail)
struct TruncatedPasteResult {
    std::string truncated_text;       ///< head + [...Truncated text #N +M lines...] + tail
    std::string placeholder_content;  ///< the truncated middle text (empty if not truncated)
};
[[nodiscard]] inline TruncatedPasteResult maybe_truncate_paste(
    std::string_view text, int paste_id) {
    constexpr std::size_t kTruncationThreshold = 10000;
    constexpr std::size_t kPreviewLength       = 1000;
    if (text.size() <= kTruncationThreshold) {
        return TruncatedPasteResult{std::string(text), ""};
    }
    const std::size_t start_len = kPreviewLength / 2;  // 500
    const std::size_t end_len   = kPreviewLength / 2;  // 500
    const std::string head(text.substr(0, start_len));
    const std::string tail(text.substr(text.size() - end_len));
    const std::string middle(
        text.substr(start_len, text.size() - start_len - end_len));
    const int elided_lines = get_pasted_text_ref_num_lines(middle);
    const std::string placeholder_ref =
        format_truncated_text_ref(paste_id, elided_lines);
    std::string out;
    out.reserve(start_len + end_len + placeholder_ref.size());
    out += head;
    out += placeholder_ref;
    out += tail;
    return TruncatedPasteResult{std::move(out), std::move(middle)};
}

/// Replace [Pasted text #N] (and [...Truncated text #N]) placeholders with
/// their stored text content.  [Image #N] refs are left untouched — they
/// become content blocks, not inline text.  The replacement is done in
/// reverse match order so earlier byte-offsets stay valid after later
/// splices.
///
/// The `PastedContent` type is intentionally not imported here — callers pass
/// a lookup lambda `get_text_content(id)` that returns std::optional<std::string>
/// (non-null = text-typed content exists, return value = expansion text;
/// nullopt = skip this ref, it's an image or unknown id).
template <typename Fn>
[[nodiscard]] std::string expand_pasted_text_refs(std::string_view input, Fn&& get_text_content) {
    auto refs = parse_references(input);
    if (refs.empty()) return std::string(input);

    std::string expanded(input);
    // Reverse-order splice so earlier offsets stay valid.
    for (std::size_t i = refs.size(); i-- > 0;) {
        const auto& ref = refs[i];
        auto text = std::invoke(std::forward<Fn>(get_text_content), ref.id);
        if (!text.has_value()) continue;  // image, unknown id, etc.
        expanded.replace(ref.index, ref.match.size(), *text);
    }
    return expanded;
}

}  // namespace loom::utils
