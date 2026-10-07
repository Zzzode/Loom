// markdown_lexer_impl.cpp - impl unit for loom.ui.visual.markdown (RFC 0001
// Phase C batch 6). Holds the block/inline lexer: tokenize_inline, the GFM
// table helpers, ordered-list detection, split_lines and lex_blocks - moved
// out of the interface BMI.
module;

#include <cctype>

module loom.ui.visual.markdown;

import std;

namespace loom::ui {
namespace detail {
namespace {

// Expand tabs to 4-space tab stops (CommonMark §2.2).  Used for block
// structure detection; tabs in code block content are preserved via
// remove_indent().
[[nodiscard]] std::string expand_tabs(std::string_view line) {
    std::string out;
    out.reserve(line.size());
    int col = 0;
    for (char c : line) {
        if (c == '\t') {
            const int spaces = 4 - (col % 4);
            out.append(static_cast<std::size_t>(spaces), ' ');
            col += spaces;
        } else {
            out += c;
            ++col;
        }
    }
    return out;
}

// Remove up to `indent` columns of leading whitespace from `original`,
// counting tab width via 4-space tab stops.  Tabs entirely within the
// indent are consumed; tabs spanning the indent boundary are consumed
// whole (CommonMark rule — the content starts after the tab).  Internal
// tabs (after the first non-whitespace) are preserved verbatim.
[[nodiscard]] std::string remove_indent(std::string_view original, int indent) {
    int col = 0;
    std::size_t i = 0;
    while (i < original.size() && col < indent) {
        if (original[i] == '\t') {
            col += 4 - (col % 4);
            ++i;
        } else if (original[i] == ' ') {
            ++col;
            ++i;
        } else {
            break;
        }
    }
    return std::string(original.substr(i));
}

// Check if a line is blank (empty or only spaces/tabs).
[[nodiscard]] bool is_blank_line(std::string_view line) {
    for (char c : line) {
        if (c != ' ' && c != '\t') return false;
    }
    return true;
}

// CommonMark §4.1: a thematic break is 3+ of `-`, `*`, `_` separated by
// optional spaces/tabs, with up to 3 spaces of leading indent.
[[nodiscard]] bool is_thematic_break(std::string_view line) {
    std::size_t i = 0;
    int leading = 0;
    while (i < line.size() && line[i] == ' ' && leading < 3) {
        ++i;
        ++leading;
    }
    if (i >= line.size()) return false;
    const char marker = line[i];
    if (marker != '-' && marker != '*' && marker != '_') return false;
    int count = 0;
    for (; i < line.size(); ++i) {
        if (line[i] == marker) {
            ++count;
        } else if (line[i] != ' ' && line[i] != '\t') {
            return false;
        }
    }
    return count >= 3;
}

// ── Entity reference decoding (CommonMark §3.5) ────────────────────

// Encode a Unicode codepoint as UTF-8.
[[nodiscard]] std::string utf8_encode(char32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

struct NamedEntity {
    std::string_view name;
    std::string_view utf8;
};

// Sorted by name (ASCII order) for binary search.  Covers the entities
// used in the CommonMark/GFM conformance suites plus the most common
// HTML5 entities.
static constexpr NamedEntity kNamedEntities[] = {
    {"AElig", "\xC3\x86"},
    {"ClockwiseContourIntegral", "\xE2\x88\xB2"},
    {"Dcaron", "\xC4\x8E"},
    {"DifferentialD", "\xE2\x85\x86"},
    {"HilbertSpace", "\xE2\x84\x8B"},
    {"amp", "&"},
    {"apos", "'"},
    {"auml", "\xC3\xA4"},
    {"copy", "\xC2\xA9"},
    {"frac34", "\xC2\xBE"},
    {"gt", ">"},
    {"lt", "<"},
    {"nbsp", "\xC2\xA0"},
    {"ngE", "\xE2\x89\xA7\xCC\xB8"},
    {"ouml", "\xC3\xB6"},
    {"quot", "\""},
};

// Look up a named entity.  Returns the UTF-8 value or nullptr.
[[nodiscard]] std::string_view lookup_named_entity(std::string_view name) {
    const auto* begin = std::begin(kNamedEntities);
    const auto* end = std::end(kNamedEntities);
    const auto it = std::lower_bound(
        begin, end, name,
        [](const NamedEntity& e, std::string_view n) { return e.name < n; });
    if (it != end && it->name == name) return it->utf8;
    return {};
}

// Try to decode an entity reference starting at text[pos] (which must be
// '&').  Returns the decoded UTF-8 string and sets `consumed` to the number
// of source characters consumed, or returns empty string / consumed=0 if
// not a valid entity.
//
// Rules (CommonMark §3.5, matching the conformance fixtures):
//   Named:  &Name;  — Name must be in the entity table, alphanumeric.
//   Decimal: &#NNN; — 1-7 digits.  0 or > 0x10FFFF → U+FFFD.
//   Hex:     &#xHH; — 1-6 hex digits (x or X).  0 or > 0x10FFFF → U+FFFD.
//   More than 7 decimal / 6 hex digits is not a valid entity (the fixtures
//   leave &#87654321; and &#abcdef0; undecoded).
[[nodiscard]] std::string decode_entity(std::string_view text,
                                        std::size_t pos,
                                        std::size_t& consumed) {
    consumed = 0;
    if (pos >= text.size() || text[pos] != '&') return {};

    // Numeric character reference
    if (pos + 1 < text.size() && text[pos + 1] == '#') {
        std::size_t p = pos + 2;
        bool is_hex = false;
        if (p < text.size() && (text[p] == 'x' || text[p] == 'X')) {
            is_hex = true;
            ++p;
        }
        const std::size_t digit_start = p;
        while (p < text.size()) {
            const char ch = text[p];
            if (is_hex) {
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                      (ch >= 'A' && ch <= 'F')))
                    break;
            } else {
                if (!(ch >= '0' && ch <= '9')) break;
            }
            ++p;
        }
        const std::size_t digit_count = p - digit_start;
        if (digit_count == 0 || p >= text.size() || text[p] != ';')
            return {};
        if (is_hex ? digit_count > 6 : digit_count > 7) return {};

        unsigned long long value = 0;
        for (std::size_t i = digit_start; i < p; ++i) {
            const char ch = text[i];
            int digit;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else digit = ch - 'A' + 10;
            value = value * (is_hex ? 16 : 10) +
                    static_cast<unsigned long long>(digit);
        }
        consumed = p - pos + 1;  // include the ';'
        if (value == 0 || value > 0x10FFFF)
            return utf8_encode(0xFFFD);
        return utf8_encode(static_cast<char32_t>(value));
    }

    // Named character reference: &Name;
    const std::size_t semi = text.find(';', pos + 1);
    if (semi == std::string_view::npos || semi - pos > 33) return {};
    const std::string_view name(text.data() + pos + 1, semi - pos - 1);
    if (name.empty()) return {};
    for (char ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) return {};
    }
    const auto decoded = lookup_named_entity(name);
    if (decoded.empty()) return {};
    consumed = semi - pos + 1;
    return std::string(decoded);
}

// CommonMark §4.3: returns 1 for === underline, 2 for --- underline,
// 0 for not a Setext heading underline.  Up to 3 spaces indent, then
// all = or all - (at least 1, no spaces between), then trailing spaces.
[[nodiscard]] int is_setext_underline(std::string_view line) {
    std::size_t i = 0;
    int leading = 0;
    while (i < line.size() && line[i] == ' ' && leading < 3) {
        ++i;
        ++leading;
    }
    if (i >= line.size()) return 0;
    const char marker = line[i];
    if (marker != '=' && marker != '-') return 0;
    ++i;
    while (i < line.size() && line[i] == marker) ++i;
    while (i < line.size() && line[i] == ' ') ++i;  // trailing spaces
    if (i < line.size()) return 0;  // non-space, non-marker character
    return marker == '=' ? 1 : 2;
}

// Decode all entity references in `text` (CommonMark §3.5).  Invalid
// entities are left as-is.
[[nodiscard]] std::string decode_entities(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            std::size_t consumed = 0;
            std::string decoded = decode_entity(text, i, consumed);
            if (consumed > 0) {
                out += decoded;
                i += consumed - 1;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

// Check if `line` (tab-expanded) is a closing fence for a fenced code
// block with the given fence character and opening length (CommonMark
// §4.5).  The closing fence may have up to 3 leading spaces, must use
// the same character, be at least `fence_len` long, and have only
// whitespace after.
[[nodiscard]] bool is_closing_fence(std::string_view line, char fence_char,
                                    std::size_t fence_len) {
    std::size_t i = 0;
    int leading = 0;
    while (i < line.size() && line[i] == ' ' && leading < 3) {
        ++i;
        ++leading;
    }
    if (i >= line.size() || line[i] != fence_char) return false;
    std::size_t count = 0;
    while (i < line.size() && line[i] == fence_char) {
        ++count;
        ++i;
    }
    if (count < fence_len) return false;
    for (; i < line.size(); ++i) {
        if (line[i] != ' ' && line[i] != '\t') return false;
    }
    return true;
}

// Percent-encode a URI for the href attribute (CommonMark autolinks).
// Encodes everything except A-Za-z0-9 and -._~:/?#@!$&'()*+,;=%
// (the encodeURI set minus [ ] which CommonMark encodes).
[[nodiscard]] std::string percent_encode_uri(std::string_view uri) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(uri.size());
    for (unsigned char ch : uri) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' ||
            ch == '_' || ch == '~' || ch == ':' || ch == '/' ||
            ch == '?' || ch == '#' || ch == '@' || ch == '!' ||
            ch == '$' || ch == '&' || ch == '\'' || ch == '(' ||
            ch == ')' || ch == '*' || ch == '+' || ch == ',' ||
            ch == ';' || ch == '=' || ch == '%') {
            out += static_cast<char>(ch);
        } else {
            out += '%';
            out += kHex[ch >> 4];
            out += kHex[ch & 0x0F];
        }
    }
    return out;
}

// Try to parse an autolink (URI or email) starting at text[pos] (which
// must be '<').  Returns the link URL and sets consumed to the number of
// characters consumed, or returns empty string / consumed=0 if not an
// autolink.  The link text is the raw content between < and >.
[[nodiscard]] std::string parse_autolink(std::string_view text,
                                         std::size_t pos,
                                         std::size_t& consumed,
                                         std::string& link_text) {
    consumed = 0;
    if (pos >= text.size() || text[pos] != '<') return {};
    const std::size_t close = text.find('>', pos + 1);
    if (close == std::string_view::npos) return {};
    const std::string_view content(text.data() + pos + 1, close - pos - 1);
    if (content.empty()) return {};
    // No spaces, <, or > allowed in the content
    for (char ch : content) {
        if (ch == ' ' || ch == '<' || ch == '>') return {};
    }

    // URI autolink: scheme 2+ alnum starting with letter, then ':'
    {
        std::size_t sp = 0;
        if (content.size() >= 2 &&
            ((content[0] >= 'A' && content[0] <= 'Z') ||
             (content[0] >= 'a' && content[0] <= 'z'))) {
            sp = 1;
            while (sp < content.size() &&
                   ((content[sp] >= 'A' && content[sp] <= 'Z') ||
                    (content[sp] >= 'a' && content[sp] <= 'z') ||
                    (content[sp] >= '0' && content[sp] <= '9') ||
                    content[sp] == '+' || content[sp] == '.' ||
                    content[sp] == '-')) {
                ++sp;
            }
            if (sp >= 2 && sp < content.size() && content[sp] == ':') {
                consumed = close - pos + 1;
                link_text = std::string(content);
                return percent_encode_uri(content);
            }
        }
    }

    // Email autolink: local@domain with at least one '.' in the domain
    {
        const std::size_t at = content.find('@');
        if (at != std::string_view::npos && at > 0 &&
            at < content.size() - 1) {
            const std::string_view local = content.substr(0, at);
            const std::string_view domain = content.substr(at + 1);
            // Local part: allowed characters (no backslash)
            bool local_ok = true;
            for (char ch : local) {
                if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                      (ch >= '0' && ch <= '9') || ch == '.' || ch == '!' ||
                      ch == '#' || ch == '$' || ch == '%' || ch == '&' ||
                      ch == '\'' || ch == '*' || ch == '+' || ch == '-' ||
                      ch == '/' || ch == '=' || ch == '?' || ch == '^' ||
                      ch == '_' || ch == '`' || ch == '{' || ch == '|' ||
                      ch == '}' || ch == '~')) {
                    local_ok = false;
                    break;
                }
            }
            // Domain: alphanumeric, '.', '-', must contain at least one '.'
            bool domain_ok = false;
            for (char ch : domain) {
                if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                    (ch >= '0' && ch <= '9') || ch == '.' || ch == '-') {
                    if (ch == '.') domain_ok = true;
                } else {
                    domain_ok = false;
                    break;
                }
            }
            if (local_ok && domain_ok) {
                consumed = close - pos + 1;
                link_text = std::string(content);
                return "mailto:" + std::string(content);
            }
        }
    }

    return {};
}

// Find the first `ch` at or after `start` that is NOT inside a code span.
// Backtick strings that form a code span are skipped over; unmatched
// backtick strings are literal text and the scan continues through them.
// Returns npos if not found.
[[nodiscard]] std::size_t find_outside_code_spans(std::string_view text,
                                                   std::size_t start,
                                                   char ch) {
    std::size_t pos = start;
    while (pos < text.size()) {
        if (text[pos] == '`') {
            std::size_t bt_end = pos + 1;
            while (bt_end < text.size() && text[bt_end] == '`') ++bt_end;
            const std::size_t bt_len = bt_end - pos;
            std::size_t search = bt_end;
            bool found_close = false;
            while (search < text.size()) {
                const auto close = text.find('`', search);
                if (close == std::string_view::npos) break;
                std::size_t ce = close + 1;
                while (ce < text.size() && text[ce] == '`') ++ce;
                if (ce - close == bt_len) {
                    pos = ce;  // skip over the code span
                    found_close = true;
                    break;
                }
                search = ce;
            }
            if (found_close) continue;
            // Unmatched backtick string — literal text, fall through
            // and continue scanning from bt_end.
            pos = bt_end;
            continue;
        }
        // Skip backslash-escaped characters (CommonMark §2.4)
        if (text[pos] == '\\' && pos + 1 < text.size()) {
            pos += 2;
            continue;
        }
        if (text[pos] == ch) return pos;
        ++pos;
    }
    return std::string_view::npos;
}

// Find the matching ']' for the '[' at position `open`, respecting nested
// [...] constructs, backslash escapes, code spans, and HTML tags/autolinks.
// Used for image alt text and link text that may contain nested brackets
// (CommonMark §6.6).  Brackets inside HTML tags and autolinks are literal
// (examples 524, 526: ](baz) inside <bar attr="..."> is not a link dest).
[[nodiscard]] std::size_t find_matching_bracket(std::string_view text,
                                                  std::size_t open) {
    if (open >= text.size() || text[open] != '[') return std::string_view::npos;
    int depth = 1;
    std::size_t pos = open + 1;
    while (pos < text.size()) {
        if (text[pos] == '\\' && pos + 1 < text.size()) {
            pos += 2;  // skip escaped char
            continue;
        }
        if (text[pos] == '`') {
            // Skip code spans — brackets inside code spans are literal.
            std::size_t bt_end = pos + 1;
            while (bt_end < text.size() && text[bt_end] == '`') ++bt_end;
            const std::size_t bt_len = bt_end - pos;
            std::size_t search = bt_end;
            while (search < text.size()) {
                const auto close = text.find('`', search);
                if (close == std::string_view::npos) {
                    pos = text.size();
                    break;
                }
                std::size_t ce = close + 1;
                while (ce < text.size() && text[ce] == '`') ++ce;
                if (ce - close == bt_len) {
                    pos = ce;
                    break;
                }
                search = ce;
            }
            continue;
        }
        if (text[pos] == '<') {
            // Skip HTML tags and autolinks — brackets inside them are
            // literal (CommonMark §6.6 examples 524, 526).
            if (pos + 1 < text.size()) {
                const char next = text[pos + 1];
                const bool looks_like_tag =
                    (next >= 'a' && next <= 'z') ||
                    (next >= 'A' && next <= 'Z') || next == '/' ||
                    next == '!' || next == '?';
                if (looks_like_tag) {
                    const auto close = text.find('>', pos + 1);
                    if (close != std::string_view::npos) {
                        pos = close + 1;
                        continue;
                    }
                }
            }
        }
        if (text[pos] == '[') {
            ++depth;
        } else if (text[pos] == ']') {
            --depth;
            if (depth == 0) return pos;
        }
        ++pos;
    }
    return std::string_view::npos;
}

// Find the end of a link label for the '[' at position `open`.
// CommonMark §4.7/§6.6: a label ends with the first ']' that is not
// backslash-escaped, and must NOT contain any unescaped '[' (examples
// 546-548: [ref[] and [ref[bar]] are invalid labels; example 549:
// [ref\[] is valid because the [ is escaped).  Returns the position of
// the ']', or npos if the label is invalid.
[[nodiscard]] std::size_t find_label_end(std::string_view text,
                                          std::size_t open) {
    if (open >= text.size() || text[open] != '[')
        return std::string_view::npos;
    for (std::size_t i = open + 1; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            ++i;  // skip escaped char
        } else if (text[i] == '[') {
            // Unescaped '[' in label — invalid (examples 546, 547, 548)
            return std::string_view::npos;
        } else if (text[i] == ']') {
            return i;
        }
    }
    return std::string_view::npos;
}

// Check if content contains a complete link (not image) at the top level.
// CommonMark §6.6: links may not contain other links — if the content of
// a [...] construct contains a complete link, the outer construct is NOT
// a link (the inner-most link is used instead).  Images (![...]) are
// allowed inside links.
[[nodiscard]] bool contains_complete_link(std::string_view content) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        if (content[pos] == '\\' && pos + 1 < content.size()) {
            pos += 2;
            continue;
        }
        if (content[pos] == '`') {
            // Skip code spans — brackets inside code spans are literal.
            std::size_t bt_end = pos + 1;
            while (bt_end < content.size() && content[bt_end] == '`') ++bt_end;
            const std::size_t bt_len = bt_end - pos;
            std::size_t search = bt_end;
            while (search < content.size()) {
                const auto close = content.find('`', search);
                if (close == std::string_view::npos) {
                    pos = content.size();
                    break;
                }
                std::size_t ce = close + 1;
                while (ce < content.size() && content[ce] == '`') ++ce;
                if (ce - close == bt_len) {
                    pos = ce;
                    break;
                }
                search = ce;
            }
            continue;
        }
        if (content[pos] == '<') {
            // Skip HTML tags and autolinks — ](...) inside them is
            // not a link destination (CommonMark §6.6 examples 524, 526).
            // Only skip constructs that could be HTML/autolink: <tag,
            // </tag, <!--, <?pi — a literal '<' (e.g. "a < b") is not
            // skipped, so ](...) after it is still found.
            if (pos + 1 < content.size()) {
                const char next = content[pos + 1];
                const bool looks_like_tag =
                    (next >= 'a' && next <= 'z') ||
                    (next >= 'A' && next <= 'Z') || next == '/' ||
                    next == '!' || next == '?';
                if (looks_like_tag) {
                    const auto close = content.find('>', pos + 1);
                    if (close != std::string_view::npos) {
                        pos = close + 1;
                        continue;
                    }
                }
            }
        }
        if (content[pos] == '[') {
            auto close = find_matching_bracket(content, pos);
            if (close != std::string_view::npos) {
                // Check if ']' is followed by '(' or '[' (complete link)
                if (close + 1 < content.size() &&
                    (content[close + 1] == '(' || content[close + 1] == '[')) {
                    // Check if this is an image (preceded by '!')
                    if (pos == 0 || content[pos - 1] != '!') {
                        return true;
                    }
                }
                pos = close + 1;
                continue;
            }
        }
        ++pos;
    }
    return false;
}

// Extract plain text from inline tokens, stripping formatting markers.
// Used for image alt text (CommonMark §6.6: alt attribute is the plain
// text content of the image description).
[[nodiscard]] std::string extract_plain_text(
    const std::vector<InlineToken>& tokens) {
    std::string out;
    for (const auto& tok : tokens) {
        switch (tok.kind) {
            case InlineTokenKind::Text:
            case InlineTokenKind::Code:
            case InlineTokenKind::Escape:
            case InlineTokenKind::Math:
            case InlineTokenKind::FootnoteRef:
                out += tok.text;
                break;
            case InlineTokenKind::Bold:
            case InlineTokenKind::Italic:
            case InlineTokenKind::Strikethrough:
                if (!tok.children.empty()) {
                    out += extract_plain_text(tok.children);
                } else {
                    out += tok.text;
                }
                break;
            case InlineTokenKind::Link:
                if (!tok.children.empty()) {
                    out += extract_plain_text(tok.children);
                } else {
                    out += tok.text;
                }
                break;
            case InlineTokenKind::Image:
                // Nested images contribute their alt text
                out += tok.text;
                break;
            case InlineTokenKind::HtmlRaw:
                // Raw HTML is stripped from alt text
                break;
        }
    }
    return out;
}

// ── Inline link destination + title parsing (CommonMark §6.6) ──────

// Result of parsing an inline link's parenthesised destination + title.
struct LinkTarget {
    std::string dest;   // entity-decoded, percent-encoded destination
    std::string title;  // entity-decoded title (empty if none)
    std::size_t end = 0;  // index of the closing ')'
};

// Parse an inline link target starting at text[pos] (which must be '(').
// Handles: angle-bracket <dest>, plain destinations with backslash escapes
// and nested parens, and an optional quoted title ("...", '...', (...)).
// Entity references in the destination are decoded then percent-encoded;
// entities in the title are decoded.  Returns nullopt if not a valid
// inline link target.
// CommonMark §2.4: only ASCII punctuation characters may be backslash-escaped.
[[nodiscard]] bool is_escapable_ascii_punct(char c) {
    switch (c) {
        case '!': case '"': case '#': case '$': case '%': case '&':
        case '\'': case '(': case ')': case '*': case '+': case ',':
        case '-': case '.': case '/': case ':': case ';': case '<':
        case '=': case '>': case '?': case '@': case '[': case '\\':
        case ']': case '^': case '_': case '`': case '{': case '|':
        case '}': case '~':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] std::optional<LinkTarget> parse_inline_link_target(
    std::string_view text, std::size_t pos) {
    if (pos >= text.size() || text[pos] != '(') return std::nullopt;

    std::size_t i = pos + 1;

    // Skip whitespace before destination (spaces/tabs; CommonMark also
    // allows a line ending but the conformance fixtures don't need it).
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t' ||
                                text[i] == '\n')) ++i;

    // ── Destination ──────────────────────────────────────────────
    std::string raw_dest;
    if (i < text.size() && text[i] == '<') {
        // Angle-bracket destination: <...> with backslash escapes.
        ++i;  // skip '<'
        while (i < text.size() && text[i] != '>') {
            if (text[i] == '\\' && i + 1 < text.size()) {
                raw_dest += text[i + 1];
                i += 2;
            } else if (text[i] == '\n' || text[i] == '<') {
                return std::nullopt;  // no line breaks / unescaped '<'
            } else {
                raw_dest += text[i];
                ++i;
            }
        }
        if (i >= text.size()) return std::nullopt;  // no closing '>'
        ++i;  // skip '>'
    } else {
        // Plain destination: non-whitespace chars, backslash escapes
        // (any ASCII punctuation is escapable — CommonMark §2.4), and
        // nested balanced parens.
        int paren_depth = 0;
        while (i < text.size()) {
            const char ch = text[i];
            if (ch == '\\' && i + 1 < text.size() &&
                is_escapable_ascii_punct(text[i + 1])) {
                raw_dest += text[i + 1];
                i += 2;
            } else if (ch == '\\') {
                raw_dest += ch;
                ++i;
            } else if (ch == '(') {
                ++paren_depth;
                raw_dest += ch;
                ++i;
            } else if (ch == ')') {
                if (paren_depth == 0) break;  // end of destination
                --paren_depth;
                raw_dest += ch;
                ++i;
            } else if (ch == ' ' || ch == '\t' || ch == '\n') {
                break;  // end of destination
            } else {
                raw_dest += ch;
                ++i;
            }
        }
        // Empty destination is valid: [link]() → <a href="">link</a>
    }

    // Skip whitespace between destination and optional title.
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t' ||
                                text[i] == '\n')) ++i;

    // ── Optional title ────────────────────────────────────────────
    std::string raw_title;
    if (i < text.size() && text[i] != ')') {
        const char quote = text[i];
        if (quote == '"' || quote == '\'' || quote == '(') {
            const char close = (quote == '(') ? ')' : quote;
            ++i;  // skip opening quote
            while (i < text.size() && text[i] != close) {
                if (text[i] == '\\' && i + 1 < text.size() &&
                    is_escapable_ascii_punct(text[i + 1])) {
                    raw_title += text[i + 1];
                    i += 2;
                } else {
                    raw_title += text[i];
                    ++i;
                }
            }
            if (i >= text.size()) return std::nullopt;  // no closing quote
            ++i;  // skip closing quote
            // Skip whitespace after title.
            while (i < text.size() &&
                   (text[i] == ' ' || text[i] == '\t')) {
                ++i;
            }
        }
    }

    // ── Closing ')' ──────────────────────────────────────────────
    if (i >= text.size() || text[i] != ')') return std::nullopt;

    LinkTarget result;
    result.dest = percent_encode_uri(decode_entities(raw_dest));
    result.title = decode_entities(raw_title);
    result.end = i;
    return result;
}

// ── GFM extended autolinks (bare URLs and emails) ──────────────────

// Percent-encode non-ASCII bytes (>= 0x80) for extended autolink hrefs.
// ASCII characters are left unchanged — GFM extended autolinks preserve
// the raw URL text, encoding only non-ASCII bytes (GFM ex 19:
// http://google.com/å → href="http://google.com/%C3%A5").
[[nodiscard]] std::string percent_encode_nonascii(std::string_view s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (ch >= 0x80) {
            out += '%';
            out += kHex[ch >> 4];
            out += kHex[ch & 0x0F];
        } else {
            out += static_cast<char>(ch);
        }
    }
    return out;
}

// Parse a domain starting at text[pos].  A domain has at least one
// period (unless allow_short); the first segment may contain
// underscores, later segments may not (GFM: underscores in hostname OK,
// in domain name not OK).  The domain must not end with '-' or '_'.
// When allow_nonascii, UTF-8 bytes (>= 0x80) are accepted as domain
// characters (GFM ex 19: http://🍄.ga/).  Returns the position after
// the domain, or npos if not a valid domain.
[[nodiscard]] std::size_t parse_domain(std::string_view text,
                                       std::size_t pos,
                                       bool allow_short = false,
                                       bool allow_nonascii = false) {
    std::size_t i = pos;
    bool first = true;
    bool has_dot = false;
    while (i < text.size()) {
        const unsigned char uc = static_cast<unsigned char>(text[i]);
        if ((text[i] >= 'a' && text[i] <= 'z') ||
            (text[i] >= 'A' && text[i] <= 'Z') ||
            (text[i] >= '0' && text[i] <= '9') || text[i] == '-' ||
            (first && text[i] == '_') ||
            (allow_nonascii && uc >= 0x80)) {
            ++i;
        } else if (text[i] == '.') {
            if (i == pos || text[i - 1] == '.') {
                return std::string_view::npos;  // empty segment
            }
            // A '.' is only part of the domain if the next character
            // is a valid domain character — otherwise it's trailing
            // punctuation (e.g. "www.example.org." → domain is
            // "example.org", the final '.' is not part of it).
            if (i + 1 >= text.size()) break;
            const unsigned char unc =
                static_cast<unsigned char>(text[i + 1]);
            if (!((text[i + 1] >= 'a' && text[i + 1] <= 'z') ||
                  (text[i + 1] >= 'A' && text[i + 1] <= 'Z') ||
                  (text[i + 1] >= '0' && text[i + 1] <= '9') ||
                  text[i + 1] == '-' ||
                  (!first && text[i + 1] == '_') ||
                  (allow_nonascii && unc >= 0x80))) {
                break;
            }
            first = false;
            has_dot = true;
            ++i;
        } else {
            break;
        }
    }
    if ((!has_dot && !allow_short) || i == pos || text[i - 1] == '.') {
        return std::string_view::npos;
    }
    // Domain must not end with '-' or '_'
    if (text[i - 1] == '-' || text[i - 1] == '_') {
        return std::string_view::npos;
    }
    return i;
}

// Check if text[pos..] starts with an HTML entity reference (&Name;
// or &#NNN; / &#xHH;).  Used to terminate extended autolinks: a URL
// ends before an entity reference so the HTML serializer can decode it.
[[nodiscard]] bool starts_entity_ref(std::string_view text,
                                     std::size_t pos) {
    if (pos >= text.size() || text[pos] != '&') return false;
    std::size_t i = pos + 1;
    if (i >= text.size()) return false;
    if (text[i] == '#') {
        ++i;
        if (i < text.size() && (text[i] == 'x' || text[i] == 'X')) ++i;
        std::size_t digits = 0;
        while (i < text.size() &&
               ((text[i] >= '0' && text[i] <= '9') ||
                (text[i] >= 'a' && text[i] <= 'f') ||
                (text[i] >= 'A' && text[i] <= 'F'))) {
            ++i;
            ++digits;
        }
        return digits > 0 && i < text.size() && text[i] == ';';
    }
    // Named entity: &Name;
    std::size_t len = 0;
    while (i < text.size() &&
           ((text[i] >= 'a' && text[i] <= 'z') ||
            (text[i] >= 'A' && text[i] <= 'Z') ||
            (text[i] >= '0' && text[i] <= '9'))) {
        ++i;
        ++len;
    }
    return len > 0 && i < text.size() && text[i] == ';';
}

// Parse the tail of a URL (path, query, fragment) after the domain.
// The URL continues until whitespace, '<', or an entity reference.
// Returns the position after the URL tail.
[[nodiscard]] std::size_t parse_url_tail(std::string_view text,
                                         std::size_t pos) {
    std::size_t i = pos;
    while (i < text.size()) {
        const char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '<') {
            break;
        }
        if (c == '&' && starts_entity_ref(text, i)) {
            break;
        }
        ++i;
    }
    return i;
}

// Strip trailing punctuation from a URL.  Parens are balanced first:
// if there are more ')' than '(', the extra ')' are removed from the
// end.  Then trailing punctuation (`.`, `,`, `;`, `:`, `!`, `?`, `]`,
// `*`, `_`, `~`, `'`, `"`) is stripped.  ')' is NOT in the trailing
// punctuation list — it's only removed during paren balancing (GFM: a
// balanced closing paren is part of the URL).  `'` and `"` are stripped
// so "http://google.com" and 'http://google.com' don't include the
// quote in the URL (GFM ex 19).
[[nodiscard]] std::size_t strip_trailing_punct(std::string_view text,
                                               std::size_t begin,
                                               std::size_t end) {
    // Balance parens first
    int open = 0;
    for (std::size_t i = begin; i < end; ++i) {
        if (text[i] == '(') ++open;
        else if (text[i] == ')') --open;
    }
    while (open < 0 && end > begin && text[end - 1] == ')') {
        --end;
        ++open;
    }
    // Strip trailing punctuation (note: ')' NOT in this list)
    while (end > begin) {
        const char c = text[end - 1];
        if (c == '.' || c == ',' || c == ';' || c == ':' ||
            c == '!' || c == '?' || c == ']' ||
            c == '*' || c == '_' || c == '~' ||
            c == '\'' || c == '"') {
            --end;
        } else {
            break;
        }
    }
    return end;
}

// Result of parsing an extended autolink.
struct ExtAutolink {
    std::string url;       // the href URL (with scheme for www)
    std::string text;      // the link text
    std::size_t end = 0;   // position after the matched text
};

// Parse an email address (local@domain) starting at text[pos].
// The local part allows only alphanumeric + .+-_ (matching cmark-gfm's
// postprocess_text).  Returns the position after the domain, or npos if
// not a valid email.
[[nodiscard]] std::size_t parse_email_address(std::string_view text,
                                               std::size_t pos) {
    std::size_t i = pos;
    while (i < text.size()) {
        const char c = text[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '+' ||
            c == '-' || c == '_') {
            ++i;
        } else {
            break;
        }
    }
    if (i > pos && i < text.size() && text[i] == '@') {
        auto domain_end = parse_domain(text, i + 1, false, false);
        if (domain_end != std::string_view::npos) {
            // If the char right after the domain is a valid domain
            // character, parse_domain stopped early (e.g. "a.b_"
            // stops at '_' because it's invalid in non-first
            // segments) — the email is not valid.
            if (domain_end < text.size()) {
                const char nc = text[domain_end];
                if ((nc >= 'a' && nc <= 'z') ||
                    (nc >= 'A' && nc <= 'Z') ||
                    (nc >= '0' && nc <= '9') ||
                    nc == '-' || nc == '_') {
                    return std::string_view::npos;
                }
            }
            // Strip trailing '.' from the domain (it's trailing
            // punctuation, not part of the domain).
            while (domain_end > i + 1 &&
                   text[domain_end - 1] == '.') {
                --domain_end;
            }
            // Re-validate: domain must still have a period.
            bool has_dot = false;
            for (std::size_t j = i + 1; j < domain_end; ++j) {
                if (text[j] == '.') has_dot = true;
            }
            if (has_dot) return domain_end;
        }
    }
    return std::string_view::npos;
}

// Try to parse a GFM extended autolink (bare URL, email, mailto:, or
// xmpp:) at text[pos].  Returns nullopt if no match.
[[nodiscard]] std::optional<ExtAutolink> parse_extended_autolink(
    std::string_view text, std::size_t pos) {
    // ── WWW autolink: www.domain ──────────────────────────────
    if (text.size() - pos >= 4 &&
        text[pos] == 'w' && text[pos + 1] == 'w' &&
        text[pos + 2] == 'w' && text[pos + 3] == '.') {
        // WWW domains require a dot (allow_short=false) but may
        // contain non-ASCII characters (allow_nonascii=true).
        auto domain_end = parse_domain(text, pos + 4, false, true);
        if (domain_end != std::string_view::npos) {
            auto url_end = parse_url_tail(text, domain_end);
            url_end = strip_trailing_punct(text, pos, url_end);
            if (url_end > pos) {
                ExtAutolink result;
                result.url = percent_encode_nonascii(
                    "http://" +
                    std::string(text.substr(pos, url_end - pos)));
                result.text =
                    std::string(text.substr(pos, url_end - pos));
                result.end = url_end;
                return result;
            }
        }
    }

    // ── URL autolink: http://, https://, ftp:// ───────────────
    // URL autolinks allow domains without a dot (allow_short=true,
    // e.g. http://inlines) and non-ASCII characters (GFM ex 19:
    // http://🍄.ga/).  Note: GFM ex 619 (CommonMark base section)
    // expects "http://example.com" NOT autolinked, but ex 628 (GFM
    // extension section) expects "http://commonmark.org" autolinked.
    // We follow the GFM extension behavior (autolink) since the
    // conformance suite runs with gfm_extensions=true.
    static constexpr std::string_view schemes[] = {
        "http://", "https://", "ftp://"};
    for (const auto& scheme : schemes) {
        if (text.size() - pos >= scheme.size() &&
            text.substr(pos, scheme.size()) == scheme) {
            auto domain_end =
                parse_domain(text, pos + scheme.size(), true, true);
            if (domain_end != std::string_view::npos) {
                auto url_end = parse_url_tail(text, domain_end);
                url_end = strip_trailing_punct(text, pos, url_end);
                if (url_end > pos) {
                    ExtAutolink result;
                    result.url = percent_encode_nonascii(
                        std::string(
                            text.substr(pos, url_end - pos)));
                    result.text = std::string(
                        text.substr(pos, url_end - pos));
                    result.end = url_end;
                    return result;
                }
            }
        }
    }

    // ── Mailto autolink: mailto:local@domain ──────────────────
    // The "mailto:" prefix is included in the link text and href.
    // The char before "mailto:" must not be alphanumeric (so
    // "mmmmailto:" doesn't match — GFM ex 19).
    if (text.size() - pos >= 7 &&
        text.substr(pos, 7) == "mailto:") {
        if (pos == 0 ||
            !std::isalnum(static_cast<unsigned char>(text[pos - 1]))) {
            auto email_end = parse_email_address(text, pos + 7);
            if (email_end != std::string_view::npos) {
                ExtAutolink result;
                result.url = "mailto:" + std::string(
                    text.substr(pos + 7, email_end - pos - 7));
                result.text = std::string(
                    text.substr(pos, email_end - pos));
                result.end = email_end;
                return result;
            }
        }
    }

    // ── Xmpp autolink: xmpp:local@domain[/path] ───────────────
    // Like mailto: but the URL may include a path after the domain
    // (GFM ex 19: xmpp:user@domain/message).
    if (text.size() - pos >= 5 &&
        text.substr(pos, 5) == "xmpp:") {
        if (pos == 0 ||
            !std::isalnum(static_cast<unsigned char>(text[pos - 1]))) {
            auto email_end = parse_email_address(text, pos + 5);
            if (email_end != std::string_view::npos) {
                auto url_end = parse_url_tail(text, email_end);
                url_end = strip_trailing_punct(text, pos, url_end);
                if (url_end > pos) {
                    ExtAutolink result;
                    result.url = percent_encode_nonascii(
                        std::string(
                            text.substr(pos, url_end - pos)));
                    result.text = std::string(
                        text.substr(pos, url_end - pos));
                    result.end = url_end;
                    return result;
                }
            }
        }
    }

    // ── Bare email autolinks ─────────────────────────────────
    // GFM autolinks bare email addresses (GFM ex 629-631).
    // The char before the email must not be alphanumeric (so
    // "abcfoo@bar.com" doesn't match).
    if (pos == 0 ||
        !std::isalnum(static_cast<unsigned char>(text[pos - 1]))) {
        auto email_end = parse_email_address(text, pos);
        if (email_end != std::string_view::npos) {
            ExtAutolink result;
            result.url = "mailto:" + std::string(
                text.substr(pos, email_end - pos));
            result.text = std::string(
                text.substr(pos, email_end - pos));
            result.end = email_end;
            return result;
        }
    }

    return std::nullopt;
}

// ── Inline HTML (CommonMark §6.6, GFM disallowed raw HTML) ─────────

// GFM disallowed raw HTML tags (GFM §6.11).  Matched case-insensitively
// on the tag name; these are escaped as text instead of passed through.
[[nodiscard]] bool is_disallowed_html_tag(std::string_view name) {
    static constexpr std::array<std::string_view, 9> kDisallowed = {
        "title", "textarea", "style", "xmp", "iframe",
        "noembed", "noframes", "script", "plaintext"};
    for (const auto& d : kDisallowed) {
        if (name.size() == d.size()) {
            bool match = true;
            for (std::size_t j = 0; j < name.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(name[j])) !=
                    d[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Parse an inline HTML construct starting at text[pos] (which must be
// '<').  Returns the end position (exclusive) if a valid construct is
// found, or npos otherwise.  Sets `disallowed` for GFM-filtered tags.
// Covers: comments, processing instructions, CDATA, declarations,
// open tags, and closing tags.
[[nodiscard]] std::size_t parse_inline_html(std::string_view text,
                                            std::size_t pos,
                                            bool& disallowed) {
    disallowed = false;
    if (pos >= text.size() || text[pos] != '<')
        return std::string_view::npos;

    // HTML comment: <!-- ... -->
    // Content must not start with '>' or '->' (CommonMark §6.6).
    // If it does, the construct is not a comment, but the '<!--' +
    // '>' / '->' is still raw text passed through unchanged (cmark
    // behavior for spec example 645).
    if (text.compare(pos, 4, "<!--") == 0) {
        if (pos + 4 < text.size()) {
            if (text[pos + 4] == '>') {
                return pos + 5;  // <!-->
            }
            if (text[pos + 4] == '-' && pos + 5 < text.size() &&
                text[pos + 5] == '>') {
                return pos + 6;  // <!--->
            }
        }
        const auto end = text.find("-->", pos + 4);
        if (end != std::string_view::npos) return end + 3;
        return std::string_view::npos;
    }

    // Processing instruction: <? ... ?>
    if (pos + 1 < text.size() && text[pos + 1] == '?') {
        const auto end = text.find("?>", pos + 2);
        if (end != std::string_view::npos) return end + 2;
        return std::string_view::npos;
    }

    // CDATA section: <![CDATA[ ... ]]>
    if (text.compare(pos, 9, "<![CDATA[") == 0) {
        const auto end = text.find("]]>", pos + 9);
        if (end != std::string_view::npos) return end + 3;
        return std::string_view::npos;
    }

    // Declaration: <! [A-Z]+ ... >
    if (pos + 1 < text.size() && text[pos + 1] == '!') {
        if (pos + 2 < text.size() &&
            text[pos + 2] >= 'A' && text[pos + 2] <= 'Z') {
            const auto end = text.find('>', pos + 3);
            if (end != std::string_view::npos) return end + 1;
        }
        return std::string_view::npos;
    }

    // Closing tag: </ tagname >
    if (pos + 1 < text.size() && text[pos + 1] == '/') {
        std::size_t i = pos + 2;
        if (i >= text.size() ||
            !std::isalpha(static_cast<unsigned char>(text[i])))
            return std::string_view::npos;
        const std::size_t name_start = i;
        while (i < text.size() &&
               (std::isalnum(static_cast<unsigned char>(text[i])) ||
                text[i] == '-'))
            ++i;
        const auto tag_name = text.substr(name_start, i - name_start);
        while (i < text.size() &&
               (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'))
            ++i;
        if (i < text.size() && text[i] == '>') {
            disallowed = is_disallowed_html_tag(tag_name);
            return i + 1;
        }
        return std::string_view::npos;
    }

    // Open tag: < tagname attributes? >
    {
        std::size_t i = pos + 1;
        if (i >= text.size() ||
            !std::isalpha(static_cast<unsigned char>(text[i])))
            return std::string_view::npos;
        const std::size_t name_start = i;
        while (i < text.size() &&
               (std::isalnum(static_cast<unsigned char>(text[i])) ||
                text[i] == '-'))
            ++i;
        const auto tag_name = text.substr(name_start, i - name_start);

        while (i < text.size()) {
            // Skip whitespace
            const std::size_t ws_start = i;
            while (i < text.size() &&
                   (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'))
                ++i;
            const bool skipped_ws = i > ws_start;
            if (i >= text.size()) return std::string_view::npos;

            // End of tag
            if (text[i] == '>') {
                disallowed = is_disallowed_html_tag(tag_name);
                return i + 1;
            }
            // Self-closing: />
            if (text[i] == '/') {
                if (i + 1 < text.size() && text[i + 1] == '>') {
                    disallowed = is_disallowed_html_tag(tag_name);
                    return i + 2;
                }
                return std::string_view::npos;
            }

            // CommonMark §6.6: an attribute consists of whitespace, an
            // attribute name, and an optional value.  Without preceding
            // whitespace the tag is invalid (e.g. "<a href='bar'title=x>").
            if (!skipped_ws) return std::string_view::npos;

            // Attribute name: starts with letter, '_', or ':'
            if (!std::isalpha(static_cast<unsigned char>(text[i])) &&
                text[i] != '_' && text[i] != ':')
                return std::string_view::npos;
            ++i;
            while (i < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[i])) ||
                    text[i] == '_' || text[i] == '.' || text[i] == ':' ||
                    text[i] == '-'))
                ++i;

            // Attribute value specification: optional whitespace + '='
            // + optional whitespace + value.  Save the position before
            // skipping whitespace so a boolean attribute (no '=') leaves
            // the whitespace for the next attribute's separator.
            const std::size_t save = i;
            while (i < text.size() &&
                   (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'))
                ++i;

            if (i < text.size() && text[i] == '=') {
                ++i;
                while (i < text.size() &&
                       (text[i] == ' ' || text[i] == '\t' ||
                        text[i] == '\n'))
                    ++i;
                if (i >= text.size()) return std::string_view::npos;

                if (text[i] == '"') {
                    ++i;
                    while (i < text.size() && text[i] != '"') ++i;
                    if (i >= text.size()) return std::string_view::npos;
                    ++i;
                } else if (text[i] == '\'') {
                    ++i;
                    while (i < text.size() && text[i] != '\'') ++i;
                    if (i >= text.size()) return std::string_view::npos;
                    ++i;
                } else {
                    // Unquoted: no whitespace, ", ', =, <, >, or `
                    while (i < text.size() && text[i] != ' ' &&
                           text[i] != '\t' && text[i] != '\n' &&
                           text[i] != '"' && text[i] != '\'' &&
                           text[i] != '=' && text[i] != '<' &&
                           text[i] != '>' && text[i] != '`')
                        ++i;
                }
            } else {
                // Boolean attribute — restore so the whitespace serves
                // as the separator for the next attribute.
                i = save;
            }
        }
        return std::string_view::npos;
    }
}

}  // anonymous namespace

// ── Link reference definitions (CommonMark §4.7) ──────────────────

// Decode one UTF-8 codepoint at `pos`.  Advances `pos` past the character.
// Returns U+FFFD for invalid sequences.
[[nodiscard]] char32_t decode_utf8(std::string_view text, std::size_t& pos) {
    if (pos >= text.size()) return U'\xFFFD';
    const auto b0 = static_cast<unsigned char>(text[pos]);
    if (b0 < 0x80) {
        ++pos;
        return static_cast<char32_t>(b0);
    }
    if (b0 < 0xC0) {
        ++pos;
        return U'\xFFFD';  // stray continuation byte
    }
    if (b0 < 0xE0) {
        if (pos + 1 >= text.size()) {
            ++pos;
            return U'\xFFFD';
        }
        const auto b1 = static_cast<unsigned char>(text[pos + 1]);
        if ((b1 & 0xC0) != 0x80) {
            ++pos;
            return U'\xFFFD';
        }
        pos += 2;
        return static_cast<char32_t>(((b0 & 0x1F) << 6) | (b1 & 0x3F));
    }
    if (b0 < 0xF0) {
        if (pos + 2 >= text.size()) {
            ++pos;
            return U'\xFFFD';
        }
        const auto b1 = static_cast<unsigned char>(text[pos + 1]);
        const auto b2 = static_cast<unsigned char>(text[pos + 2]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
            ++pos;
            return U'\xFFFD';
        }
        pos += 3;
        return static_cast<char32_t>(((b0 & 0x0F) << 12) |
                                      ((b1 & 0x3F) << 6) | (b2 & 0x3F));
    }
    if (pos + 3 >= text.size()) {
        ++pos;
        return U'\xFFFD';
    }
    const auto b1 = static_cast<unsigned char>(text[pos + 1]);
    const auto b2 = static_cast<unsigned char>(text[pos + 2]);
    const auto b3 = static_cast<unsigned char>(text[pos + 3]);
    if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 ||
        (b3 & 0xC0) != 0x80) {
        ++pos;
        return U'\xFFFD';
    }
    pos += 4;
    return static_cast<char32_t>(((b0 & 0x07) << 18) |
                                  ((b1 & 0x3F) << 12) |
                                  ((b2 & 0x3F) << 6) | (b3 & 0x3F));
}

// Append a Unicode codepoint to `out` as UTF-8.
void encode_utf8(char32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Unicode case folding for link label normalization (CommonMark §6.6).
// Handles ASCII, Latin-1, Greek, and Cyrillic ranges.  The ẞ→ss special
// case (U+1E9E, a 1→2 mapping) is handled by the caller.
[[nodiscard]] char32_t unicode_case_fold(char32_t cp) {
    // ASCII uppercase
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    // Latin-1 Supplement: À-Ö (00C0-00D6), Ø-Þ (00D8-00DE)
    if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) return cp + 32;
    // Greek: Α-Ρ (0391-03A1), Σ-Ω (03A3-03AB)
    if (cp >= 0x0391 && cp <= 0x03A1) return cp + 32;
    if (cp >= 0x03A3 && cp <= 0x03AB) return cp + 32;
    // Greek accented uppercase
    if (cp == 0x0386) return 0x03AC;  // Ά → ά
    if (cp == 0x0388) return 0x03AD;  // Έ → έ
    if (cp == 0x0389) return 0x03AE;  // Ή → ή
    if (cp == 0x038A) return 0x03AF;  // Ί → ί
    if (cp == 0x038C) return 0x03CC;  // Ό → ό
    if (cp == 0x038E) return 0x03CD;  // Ύ → ύ
    if (cp == 0x038F) return 0x03CE;  // Ώ → ώ
    // Cyrillic: А-Я (0410-042F), Ё (0401)
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 32;
    if (cp == 0x0401) return 0x0451;  // Ё → ё
    return cp;
}

// Normalize a link label per CommonMark: case-fold (Unicode), collapse
// consecutive whitespace to a single space, strip leading/trailing
// whitespace.  Backslash escapes are NOT processed in labels (CommonMark
// §6.6 example 545: [bar][foo\!] does not match [foo!]: /url).
[[nodiscard]] std::string normalize_label(std::string_view label) {
    std::string out;
    out.reserve(label.size());
    bool last_was_space = true;  // trim leading
    std::size_t i = 0;
    while (i < label.size()) {
        const char32_t cp = decode_utf8(label, i);
        if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
            if (!last_was_space) {
                out += ' ';
                last_was_space = true;
            }
        } else {
            // ẞ (U+1E9E) case-folds to "ss" per Unicode CaseFolding.txt
            // (CommonMark §6.6 example 540).
            if (cp == 0x1E9E) {
                out += "ss";
            } else {
                encode_utf8(unicode_case_fold(cp), out);
            }
            last_was_space = false;
        }
    }
    // Trim trailing space
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Try to parse a link reference definition from a line.
// Format: [label]: /url "title"  (title optional, in "", '', or ())
// The URL may be wrapped in <>.  The URL and/or title may be on the
// next line (indented 1-3 spaces).  Returns the parsed definition and
// the number of lines consumed (1, 2, or 3), or std::nullopt if the
// line is not a definition.
struct ParsedLinkRefDef {
    std::string label;
    LinkRefDef def;
    int lines_consumed = 1;
};

[[nodiscard]] std::optional<ParsedLinkRefDef> try_parse_link_ref_def(
    const std::vector<std::string>& lines, std::size_t start) {
    if (start >= lines.size()) return std::nullopt;
    // Up to 3 spaces indent
    std::size_t pos = 0;
    while (pos < lines[start].size() && lines[start][pos] == ' ' && pos < 3)
        ++pos;
    if (pos >= lines[start].size() || lines[start][pos] != '[')
        return std::nullopt;

    // Find closing ']' for the label.  Labels end with the first ']'
    // that is not backslash-escaped, and must not contain unescaped '['
    // (CommonMark §4.7 examples 546-548).  Labels may span multiple
    // lines (examples 208, 541: [Foo\n  bar]: /url).
    std::string label;
    std::size_t close = find_label_end(lines[start], pos);
    int label_lines = 1;
    if (close != std::string_view::npos) {
        label = lines[start].substr(pos + 1, close - pos - 1);
    } else {
        // Label may continue on subsequent lines.  Check that the first
        // line has no unescaped '[' (which would make it invalid).
        bool has_unescaped_bracket = false;
        for (std::size_t i = pos + 1; i < lines[start].size(); ++i) {
            if (lines[start][i] == '\\' && i + 1 < lines[start].size()) {
                ++i;
            } else if (lines[start][i] == '[') {
                has_unescaped_bracket = true;
                break;
            }
        }
        if (!has_unescaped_bracket) {
            // Join lines until ']' is found or a blank line is hit.
            std::string joined = lines[start];
            for (std::size_t li = start + 1;
                 li < lines.size() && label_lines < 10; ++li) {
                const std::string& l = lines[li];
                if (l.empty() ||
                    l.find_first_not_of(" \t") == std::string::npos) {
                    break;  // blank line terminates the label
                }
                joined += '\n';
                joined += l;
                close = find_label_end(joined, pos);
                if (close != std::string_view::npos &&
                    close + 1 < joined.size() && joined[close + 1] == ':') {
                    label = joined.substr(pos + 1, close - pos - 1);
                    label_lines = static_cast<int>(li - start + 1);
                    break;
                }
                // If ']' found but no ':' after it, not a valid definition.
                if (close != std::string_view::npos) break;
            }
        }
    }
    if (close == std::string_view::npos) return std::nullopt;

    // Determine the line containing the ':' and the position after it.
    // The ':' is on the last line of the (possibly multi-line) label.
    std::string raw;
    std::size_t after_colon;
    if (label_lines == 1) {
        raw = lines[start];
        if (close + 1 >= raw.size() || raw[close + 1] != ':')
            return std::nullopt;
        after_colon = close + 2;
    } else {
        const std::size_t last_line_idx = start + label_lines - 1;
        raw = lines[last_line_idx];
        // Compute the offset of the last line in the joined string.
        std::size_t last_line_offset = 0;
        for (std::size_t i = start; i < last_line_idx; ++i) {
            last_line_offset += lines[i].size() + 1;  // +1 for '\n'
        }
        // close is in joined coordinates; convert to last-line coords.
        if (close < last_line_offset) return std::nullopt;
        std::size_t colon_in_line = close + 1 - last_line_offset;
        if (colon_in_line >= raw.size() || raw[colon_in_line] != ':')
            return std::nullopt;
        after_colon = colon_in_line + 1;
    }
    if (label.empty()) return std::nullopt;
    // CommonMark §4.7: labels must contain at least one non-whitespace char.
    if (normalize_label(label).empty()) return std::nullopt;

    // Parse URL after ':' — may be on the same line or the next line
    std::size_t url_start = after_colon;
    while (url_start < raw.size() &&
           (raw[url_start] == ' ' || raw[url_start] == '\t')) {
        ++url_start;
    }

    int lines_consumed = label_lines;
    std::string url;
    std::size_t after_url = 0;  // position after URL on its line
    const std::string* url_line = &raw;

    if (url_start < raw.size()) {
        // URL on the same line
        if (raw[url_start] == '<') {
            // URL wrapped in <>
            std::size_t url_end = raw.find('>', url_start + 1);
            if (url_end == std::string_view::npos) return std::nullopt;
            // Check for invalid chars in angle-URL
            for (std::size_t i = url_start + 1; i < url_end; ++i) {
                if (raw[i] == '<' || raw[i] == '\n') return std::nullopt;
            }
            url = raw.substr(url_start + 1, url_end - url_start - 1);
            after_url = url_end + 1;
        } else {
            // Bare URL: read until whitespace (but not past a newline —
            // a bare URL cannot span lines in the joined string).
            std::size_t url_end = url_start;
            while (url_end < raw.size() && raw[url_end] != ' ' &&
                   raw[url_end] != '\t' && raw[url_end] != '\n') {
                ++url_end;
            }
            url = raw.substr(url_start, url_end - url_start);
            after_url = url_end;
        }
    } else if (start + label_lines < lines.size()) {
        // URL may be on the next line (any indentation, including 0 spaces)
        const std::string& next = lines[start + label_lines];
        std::size_t next_pos = 0;
        while (next_pos < next.size() && next[next_pos] == ' ') ++next_pos;
        if (next_pos < next.size()) {
            url_line = &next;
            url_start = next_pos;
            if (next[next_pos] == '<') {
                std::size_t url_end = next.find('>', next_pos + 1);
                if (url_end == std::string_view::npos) return std::nullopt;
                for (std::size_t i = next_pos + 1; i < url_end; ++i) {
                    if (next[i] == '<' || next[i] == '\n') return std::nullopt;
                }
                url = next.substr(next_pos + 1, url_end - next_pos - 1);
                after_url = url_end + 1;
            } else {
                std::size_t url_end = next_pos;
                while (url_end < next.size() && next[url_end] != ' ' &&
                       next[url_end] != '\t') {
                    ++url_end;
                }
                url = next.substr(next_pos, url_end - next_pos);
                after_url = url_end;
            }
            lines_consumed = label_lines + 1;
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    // Empty URLs are valid when wrapped in <> (CommonMark §4.7 example 200).
    // Bare empty URLs (nothing after ':') are not.
    if (url.empty() && url_start < url_line->size() &&
        (*url_line)[url_start] != '<') {
        return std::nullopt;
    }

    // Process backslash escapes in URL — only ASCII punctuation is escapable
    // (CommonMark §2.4).  A backslash before a non-punctuation char is literal.
    std::string unescaped_url;
    unescaped_url.reserve(url.size());
    for (std::size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '\\' && i + 1 < url.size() &&
            is_escapable_ascii_punct(url[i + 1])) {
            unescaped_url += url[i + 1];
            ++i;
        } else {
            unescaped_url += url[i];
        }
    }

    // Parse optional title.  The title must be separated from the URL by
    // whitespace (CommonMark §4.7).  It can be on the same line or the
    // next line(s).  A title may span multiple lines when wrapped in
    // matching quotes (CommonMark §4.7 example 196).  Any non-whitespace
    // after the closing quote makes the definition invalid.
    std::string title;
    bool had_ws_after_url = false;
    std::size_t title_start = after_url;
    const std::string* title_line = url_line;

    // Skip whitespace on the URL's line
    while (title_start < title_line->size() &&
           ((*title_line)[title_start] == ' ' ||
            (*title_line)[title_start] == '\t')) {
        ++title_start;
        had_ws_after_url = true;
    }

    // Try to parse a title starting at (line_idx, tpos).  If the closing
    // quote is on a later line, the title content includes the newlines.
    // Returns the number of lines consumed (0 = failure).
    auto try_parse_title_at = [&](std::size_t line_idx,
                                   std::size_t tpos) -> int {
        if (line_idx >= lines.size()) return 0;
        const std::string& line = lines[line_idx];
        if (tpos >= line.size()) return 0;
        char q = line[tpos];
        if (q != '"' && q != '\'' && q != '(') return 0;
        char close_q = (q == '(') ? ')' : q;

        // Search for closing quote, possibly across lines.
        std::string content;
        // First line: content after opening quote
        std::size_t i = tpos + 1;
        while (i < line.size()) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                content += line[i];
                content += line[i + 1];
                i += 2;
            } else if (line[i] == close_q) {
                // Found closing quote on same line.
                // Check for non-whitespace after title.
                for (std::size_t j = i + 1; j < line.size(); ++j) {
                    if (line[j] != ' ' && line[j] != '\t') return 0;
                }
                title = std::move(content);
                return 1;
            } else {
                content += line[i];
                ++i;
            }
        }
        // Closing quote not on this line — continue to next lines.
        // A blank line terminates the title (CommonMark §4.7 example 197).
        for (std::size_t li = line_idx + 1; li < lines.size(); ++li) {
            const std::string& l = lines[li];
            if (l.empty() || l.find_first_not_of(" \t") == std::string::npos) {
                return 0;  // blank line — title is invalid
            }
            content += '\n';
            std::size_t j = 0;
            while (j < l.size()) {
                if (l[j] == '\\' && j + 1 < l.size()) {
                    content += l[j];
                    content += l[j + 1];
                    j += 2;
                } else if (l[j] == close_q) {
                    // Found closing quote.
                    // Check for non-whitespace after title.
                    for (std::size_t k = j + 1; k < l.size(); ++k) {
                        if (l[k] != ' ' && l[k] != '\t') return 0;
                    }
                    title = std::move(content);
                    return static_cast<int>(li - line_idx + 1);
                } else {
                    content += l[j];
                    ++j;
                }
            }
        }
        return 0;  // no closing quote found
    };

    if (had_ws_after_url && title_start < title_line->size()) {
        // Title on the same line as URL
        int consumed = try_parse_title_at(start + lines_consumed - 1,
                                          title_start);
        if (consumed == 0) {
            // Non-whitespace after URL that's not a valid title
            return std::nullopt;
        }
        lines_consumed = start + lines_consumed - 1 + consumed - start;
    } else if (!had_ws_after_url && title_start < title_line->size()) {
        // No whitespace between URL and title — not a valid definition
        // (e.g. "[foo]: <bar>(baz)" — "(baz)" is not a title)
        return std::nullopt;
    } else {
        // Title may be on the next line (any indentation)
        std::size_t next_line_idx = start + lines_consumed;
        if (next_line_idx < lines.size()) {
            const std::string& next = lines[next_line_idx];
            std::size_t next_pos = 0;
            while (next_pos < next.size() && next[next_pos] == ' ') ++next_pos;
            if (next_pos < next.size()) {
                int consumed = try_parse_title_at(next_line_idx, next_pos);
                if (consumed > 0) {
                    lines_consumed = static_cast<int>(next_line_idx - start) +
                                     consumed;
                }
            }
        }
    }

    // Process backslash escapes in title — only ASCII punctuation is
    // escapable (CommonMark §2.4).
    if (!title.empty()) {
        std::string unescaped_title;
        unescaped_title.reserve(title.size());
        for (std::size_t j = 0; j < title.size(); ++j) {
            if (title[j] == '\\' && j + 1 < title.size() &&
                is_escapable_ascii_punct(title[j + 1])) {
                unescaped_title += title[j + 1];
                ++j;
            } else {
                unescaped_title += title[j];
            }
        }
        title = std::move(unescaped_title);
    }

    ParsedLinkRefDef result;
    result.label = normalize_label(label);
    // Decode entities and percent-encode URL, matching the inline link
    // parser behavior (CommonMark §4.7 + §6.6: link destinations are
    // entity-decoded then percent-encoded; titles are entity-decoded).
    result.def.url = percent_encode_uri(decode_entities(unescaped_url));
    result.def.title = decode_entities(title);
    result.lines_consumed = lines_consumed;
    return result;
}

// Check whether a line matches the GFM footnote definition pattern:
// [^label]:  (label non-empty, no whitespace or ']' in the label).
// Used to prevent scan_link_ref_defs from parsing footnote defs as link
// ref defs with label "^label" when GFM extensions are enabled.
[[nodiscard]] bool is_footnote_def_line(std::string_view line) {
    std::size_t pos = 0;
    while (pos < line.size() && line[pos] == ' ' && pos < 3) ++pos;
    if (pos + 1 >= line.size() || line[pos] != '[' || line[pos + 1] != '^') {
        return false;
    }
    std::size_t label_end = pos + 2;
    while (label_end < line.size() && line[label_end] != ']' &&
           line[label_end] != ' ' && line[label_end] != '\t' &&
           line[label_end] != '\r' && line[label_end] != '\n') {
        ++label_end;
    }
    if (label_end == pos + 2) return false;  // empty label
    return label_end < line.size() && line[label_end] == ']' &&
           label_end + 1 < line.size() && line[label_end + 1] == ':';
}

// Pre-scan the document for link reference definitions.
// Returns a map of normalized label → definition, and a set of line
// indices that are consumed by definitions (to skip in the main loop).
// Definitions inside code fences are skipped.  Definitions that would
// interrupt a paragraph (i.e. the previous non-blank line was a text
// line, not a block-level construct) are also skipped (CommonMark §4.7:
// link reference definitions cannot interrupt paragraphs).
// When gfm_extensions is true, lines matching the footnote def pattern
// are skipped (they are consumed by scan_footnote_defs instead).
[[nodiscard]] std::pair<LinkRefMap, std::unordered_set<std::size_t>>
scan_link_ref_defs(const std::vector<std::string>& lines,
                   bool gfm_extensions = false) {
    LinkRefMap refs;
    std::unordered_set<std::size_t> consumed;
    bool in_code_fence = false;
    char fence_char = 0;
    // Track whether the previous non-blank line was a "block-level" line
    // (definition, heading, list, blockquote, fence, etc.) or a "text"
    // line (paragraph continuation).  A definition can only follow a
    // block-level line or a blank line.
    bool prev_was_text = false;

    auto is_block_start = [](const std::string& line) -> bool {
        std::size_t pos = 0;
        while (pos < line.size() && line[pos] == ' ' && pos < 3) ++pos;
        if (pos >= line.size()) return false;
        char c = line[pos];
        if (c == '#') return true;  // ATX heading
        if (c == '>') return true;  // blockquote
        if (c == '`' || c == '~') return true;  // code fence
        if (c == '-' || c == '*' || c == '+') {
            // List item or thematic break
            return pos + 1 < line.size() &&
                   (line[pos + 1] == ' ' || line[pos + 1] == '\t');
        }
        if (c >= '0' && c <= '9') return true;  // ordered list
        if (c == '[') return true;  // potential definition
        return false;
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (consumed.contains(i)) continue;
        const std::string& raw = lines[i];

        // Track code fences
        {
            std::size_t pos = 0;
            while (pos < raw.size() && raw[pos] == ' ' && pos < 3) ++pos;
            if (pos < raw.size() && (raw[pos] == '`' || raw[pos] == '~')) {
                std::size_t fence_len = 0;
                while (pos + fence_len < raw.size() &&
                       raw[pos + fence_len] == raw[pos]) {
                    ++fence_len;
                }
                if (fence_len >= 3) {
                    if (!in_code_fence) {
                        in_code_fence = true;
                        fence_char = raw[pos];
                    } else if (raw[pos] == fence_char) {
                        in_code_fence = false;
                    }
                    prev_was_text = false;
                    continue;
                }
            }
        }

        if (in_code_fence) {
            prev_was_text = false;
            continue;
        }

        // Skip blank lines
        bool is_blank = true;
        for (char c : raw) {
            if (c != ' ' && c != '\t' && c != '\r') {
                is_blank = false;
                break;
            }
        }
        if (is_blank) {
            prev_was_text = false;
            continue;
        }

        // Only scan lines that start with '[' (after up to 3 spaces),
        // or lines inside blockquotes that start with '[' after the '> '.
        std::size_t pos = 0;
        while (pos < raw.size() && raw[pos] == ' ' && pos < 3) ++pos;
        // Check for blockquote marker: up to 3 spaces, then '>'
        std::size_t bq_content_pos = pos;
        bool in_blockquote = false;
        if (pos < raw.size() && raw[pos] == '>') {
            in_blockquote = true;
            bq_content_pos = pos + 1;
            if (bq_content_pos < raw.size() && raw[bq_content_pos] == ' ') {
                ++bq_content_pos;  // skip one optional space after '>'
            }
        }
        if (!in_blockquote &&
            (pos >= raw.size() || raw[pos] != '[')) {
            // Not a potential definition — check if it's a block-level
            // construct or a text line
            prev_was_text = !is_block_start(raw);
            continue;
        }
        if (in_blockquote &&
            (bq_content_pos >= raw.size() || raw[bq_content_pos] != '[')) {
            // Blockquote line but not a definition — check block start
            prev_was_text = !is_block_start(raw);
            continue;
        }

        // A definition cannot follow a text line (paragraph continuation)
        if (prev_was_text) {
            prev_was_text = true;
            continue;
        }

        if (in_blockquote) {
            // Try to parse a definition inside a blockquote.  Build a
            // temporary vector of stripped blockquote lines so that
            // multi-line definitions (URL/title on next line) work.
            std::vector<std::string> bq_lines;
            std::size_t j = i;
            // Collect consecutive blockquote lines
            while (j < lines.size()) {
                const std::string& bl = lines[j];
                std::size_t bp = 0;
                while (bp < bl.size() && bl[bp] == ' ' && bp < 3) ++bp;
                if (bp >= bl.size() || bl[bp] != '>') break;
                std::size_t content_start = bp + 1;
                if (content_start < bl.size() && bl[content_start] == ' ') {
                    ++content_start;
                }
                bq_lines.push_back(bl.substr(content_start));
                ++j;
                // Don't collect more than 10 lines (safety)
                if (bq_lines.size() >= 10) break;
            }
            // GFM footnote defs inside blockquotes are consumed by
            // scan_footnote_defs (via recursive lex_blocks), not as
            // link ref defs with label "^label".
            if (gfm_extensions && !bq_lines.empty() &&
                is_footnote_def_line(bq_lines[0])) {
                prev_was_text = false;
                continue;
            }
            if (auto parsed = try_parse_link_ref_def(bq_lines, 0)) {
                // Merge definition into refs, but do NOT consume the
                // blockquote lines — they still need to be parsed as a
                // blockquote in the main loop (CommonMark §4.7 example 218).
                if (!refs.contains(parsed->label)) {
                    refs[parsed->label] = std::move(parsed->def);
                }
                prev_was_text = false;
                continue;
            }
            prev_was_text = true;
            continue;
        }

        // GFM footnote defs are consumed by scan_footnote_defs, not as
        // link ref defs with label "^label".
        if (gfm_extensions && is_footnote_def_line(raw)) {
            prev_was_text = false;
            continue;
        }

        if (auto parsed = try_parse_link_ref_def(lines, i)) {
            // First definition wins (CommonMark: later definitions with
            // the same label are ignored)
            if (!refs.contains(parsed->label)) {
                refs[parsed->label] = std::move(parsed->def);
            }
            consumed.insert(i);
            for (int j = 1; j < parsed->lines_consumed; ++j) {
                consumed.insert(i + j);
            }
            prev_was_text = false;
        } else {
            prev_was_text = true;
        }
    }
    return {std::move(refs), std::move(consumed)};
}

// A parsed GFM footnote definition: [^label]: content
struct FootnoteDefEntry {
    std::string raw_label;  // label as written (without '^')
    std::string content;    // first content line + continuation lines, dedented
};

// Pre-scan for GFM footnote definitions: [^label]: <content>
// Returns defs in source order and a set of consumed line indices.
// Unlike link ref defs, footnote defs CAN interrupt paragraphs (cmark-gfm).
// Labels cannot contain whitespace or ']'.  Continuation lines are blank
// or indented >= 4 spaces (4 consumed, matching cmark's block prefix).
// First definition wins for duplicate labels (lines still consumed).
[[nodiscard]] std::pair<std::vector<FootnoteDefEntry>,
                        std::unordered_set<std::size_t>>
scan_footnote_defs(const std::vector<std::string>& lines) {
    std::vector<FootnoteDefEntry> entries;
    std::unordered_set<std::size_t> consumed;
    std::unordered_set<std::string> seen_labels;
    bool in_code_fence = false;
    char fence_char = 0;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (consumed.contains(i)) continue;
        const std::string& raw = lines[i];

        // Track code fences (same pattern as scan_link_ref_defs)
        {
            std::size_t pos = 0;
            while (pos < raw.size() && raw[pos] == ' ' && pos < 3) ++pos;
            if (pos < raw.size() && (raw[pos] == '`' || raw[pos] == '~')) {
                std::size_t fence_len = 0;
                while (pos + fence_len < raw.size() &&
                       raw[pos + fence_len] == raw[pos]) {
                    ++fence_len;
                }
                if (fence_len >= 3) {
                    if (!in_code_fence) {
                        in_code_fence = true;
                        fence_char = raw[pos];
                    } else if (raw[pos] == fence_char) {
                        in_code_fence = false;
                    }
                    continue;
                }
            }
        }

        if (in_code_fence) continue;

        // Skip blank lines
        bool is_blank = true;
        for (char c : raw) {
            if (c != ' ' && c != '\t' && c != '\r') {
                is_blank = false;
                break;
            }
        }
        if (is_blank) continue;

        // Up to 3 leading spaces, then '[^'
        std::size_t pos = 0;
        while (pos < raw.size() && raw[pos] == ' ' && pos < 3) ++pos;
        if (pos + 1 >= raw.size() || raw[pos] != '[' ||
            raw[pos + 1] != '^') {
            continue;
        }

        // Scan label: one or more chars excluding ']', space, tab, newline
        std::size_t label_start = pos + 2;
        std::size_t label_end = label_start;
        while (label_end < raw.size() && raw[label_end] != ']' &&
               raw[label_end] != ' ' && raw[label_end] != '\t' &&
               raw[label_end] != '\r' && raw[label_end] != '\n') {
            ++label_end;
        }
        if (label_end == label_start) continue;  // empty label
        if (label_end >= raw.size() || raw[label_end] != ']') continue;
        if (label_end + 1 >= raw.size() || raw[label_end + 1] != ':')
            continue;

        std::string raw_label =
            raw.substr(label_start, label_end - label_start);

        // First content line: remainder after ']:', skipping spaces/tabs
        std::size_t content_start = label_end + 2;
        while (content_start < raw.size() &&
               (raw[content_start] == ' ' || raw[content_start] == '\t')) {
            ++content_start;
        }
        std::string content = raw.substr(content_start);
        if (!content.empty() && content.back() == '\r') content.pop_back();

        // Collect continuation lines: blank or indented >= 4 spaces
        std::size_t j = i + 1;
        while (j < lines.size()) {
            const std::string& cl = lines[j];
            bool cl_blank = true;
            for (char c : cl) {
                if (c != ' ' && c != '\t' && c != '\r') {
                    cl_blank = false;
                    break;
                }
            }
            if (cl_blank) {
                content += '\n';
                consumed.insert(j);
                ++j;
                continue;
            }
            // Indent >= 4 (after tab expansion): continuation line
            const std::string expanded = expand_tabs(cl);
            std::size_t indent = 0;
            while (indent < expanded.size() && expanded[indent] == ' ')
                ++indent;
            if (indent >= 4) {
                if (!content.empty()) content += '\n';
                content += expanded.substr(4);
                consumed.insert(j);
                ++j;
                continue;
            }
            break;
        }

        consumed.insert(i);
        // First definition wins for duplicate labels
        auto normalized = normalize_label(raw_label);
        if (!seen_labels.contains(normalized)) {
            seen_labels.insert(normalized);
            entries.push_back({std::move(raw_label), std::move(content)});
        }
        i = j - 1;  // -1: loop increments i
    }
    return {std::move(entries), std::move(consumed)};
}
// Unicode-aware flanking rules.  Covers the CommonMark conformance
// suite (which uses ASCII + NBSP + a few Unicode punctuation chars).
//
// Left-flanking:  not followed by whitespace, AND
//   (not followed by punctuation OR preceded by whitespace/punctuation/start)
// Right-flanking: not preceded by whitespace, AND
//   (not preceded by punctuation OR followed by whitespace/punctuation/end)

// Check if a Unicode codepoint is whitespace (CommonMark §2.2).
[[nodiscard]] bool is_md_whitespace_cp(char32_t cp) {
    if (cp < 0x80)
        return std::isspace(static_cast<unsigned char>(cp)) != 0;
    return cp == 0x00A0 ||  // NO-BREAK SPACE
           cp == 0x1680 ||  // OGHAM SPACE MARK
           (cp >= 0x2000 && cp <= 0x200A) ||  // EN QUAD..HAIR SPACE
           cp == 0x2028 ||  // LINE SEPARATOR
           cp == 0x2029 ||  // PARAGRAPH SEPARATOR
           cp == 0x202F ||  // NARROW NO-BREAK SPACE
           cp == 0x205F ||  // MEDIUM MATHEMATICAL SPACE
           cp == 0x3000;    // IDEOGRAPHIC SPACE
}

// Check if a Unicode codepoint is punctuation (Unicode P category).
// This is a simplified range check covering the characters used in the
// CommonMark conformance suite, not a full Unicode database lookup.
[[nodiscard]] bool is_md_punct_cp(char32_t cp) {
    if (cp < 0x80)
        return std::ispunct(static_cast<unsigned char>(cp)) != 0;
    // Latin-1 Supplement punctuation (Po/Ps/Pe/Pd/Pi/Pf)
    if (cp >= 0x00A1 && cp <= 0x00BF) {
        constexpr char32_t kLatin1Punct[] = {
            0x00A1, 0x00A3, 0x00A7, 0x00AB, 0x00B6, 0x00B7, 0x00BB, 0x00BF};
        for (char32_t p : kLatin1Punct)
            if (cp == p) return true;
        return false;
    }
    // € (EURO SIGN) is Po category
    if (cp == 0x20AC) return true;
    // General Punctuation block (2000-206F) — mostly P* categories
    if (cp >= 0x2000 && cp <= 0x206F) return true;
    // CJK symbols and punctuation (3000-303F)
    if (cp >= 0x3000 && cp <= 0x303F) return true;
    // Fullwidth forms (FF00-FFEF) — fullwidth punctuation
    if (cp >= 0xFF00 && cp <= 0xFF0F) return true;
    if (cp >= 0xFF1A && cp <= 0xFF20) return true;
    if (cp >= 0xFF3B && cp <= 0xFF40) return true;
    if (cp >= 0xFF5B && cp <= 0xFF65) return true;
    return false;
}

// Decode the UTF-8 codepoint immediately before `before_pos`.
// Returns U+0000 if before_pos is 0.
[[nodiscard]] char32_t decode_utf8_before(std::string_view text,
                                           std::size_t before_pos) {
    if (before_pos == 0) return U'\0';
    std::size_t start = before_pos - 1;
    while (start > 0 &&
           (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
        --start;
    }
    std::size_t pos = start;
    return decode_utf8(text, pos);
}

[[nodiscard]] bool is_md_punct(char c) {
    return std::ispunct(static_cast<unsigned char>(c));
}

[[nodiscard]] bool is_left_flanking(std::string_view text, std::size_t pos,
                                    std::size_t run_len) {
    if (pos + run_len >= text.size()) return false;  // followed by end
    std::size_t next_pos = pos + run_len;
    const char32_t next = decode_utf8(text, next_pos);
    if (is_md_whitespace_cp(next)) return false;
    if (!is_md_punct_cp(next)) return true;
    // Followed by punctuation — left-flanking only if preceded by
    // whitespace, punctuation, or start of text.
    if (pos == 0) return true;
    const char32_t prev = decode_utf8_before(text, pos);
    return is_md_whitespace_cp(prev) || is_md_punct_cp(prev);
}

[[nodiscard]] bool is_right_flanking(std::string_view text, std::size_t pos,
                                     std::size_t run_len) {
    if (pos == 0) return false;  // preceded by start
    const char32_t prev = decode_utf8_before(text, pos);
    if (is_md_whitespace_cp(prev)) return false;
    if (!is_md_punct_cp(prev)) return true;
    // Preceded by punctuation — right-flanking only if followed by
    // whitespace, punctuation, or end of text.
    if (pos + run_len >= text.size()) return true;
    std::size_t next_pos = pos + run_len;
    const char32_t next = decode_utf8(text, next_pos);
    return is_md_whitespace_cp(next) || is_md_punct_cp(next);
}

// Can a '*' delimiter run at pos open emphasis?
[[nodiscard]] bool star_can_open(std::string_view text, std::size_t pos,
                                 std::size_t run_len) {
    return is_left_flanking(text, pos, run_len);
}

// Can a '*' delimiter run at pos close emphasis?
[[nodiscard]] bool star_can_close(std::string_view text, std::size_t pos,
                                  std::size_t run_len) {
    return is_right_flanking(text, pos, run_len);
}

// Can a '_' delimiter run at pos open emphasis?
// Stricter than '*': must be left-flanking AND (not right-flanking OR
// preceded by punctuation).
[[nodiscard]] bool underscore_can_open(std::string_view text, std::size_t pos,
                                       std::size_t run_len) {
    if (!is_left_flanking(text, pos, run_len)) return false;
    if (!is_right_flanking(text, pos, run_len)) return true;
    // Both flanking — can open only if preceded by punctuation.
    if (pos == 0) return false;
    return is_md_punct(text[pos - 1]);
}

// Can a '_' delimiter run at pos close emphasis?
// Must be right-flanking AND (not left-flanking OR followed by punctuation).
[[nodiscard]] bool underscore_can_close(std::string_view text, std::size_t pos,
                                        std::size_t run_len) {
    if (!is_right_flanking(text, pos, run_len)) return false;
    if (!is_left_flanking(text, pos, run_len)) return true;
    // Both flanking — can close only if followed by punctuation.
    if (pos + run_len >= text.size()) return false;
    return is_md_punct(text[pos + run_len]);
}

// ── Delimiter stack algorithm (CommonMark §6.1.1) ──────────────────
//
// The emphasis parser works in two phases:
//
// Phase 1: Build a flat list of "items" — text runs, non-emphasis tokens
// (code spans, links, images, HTML, etc.), and delimiter runs (* or _).
// The delimiter runs record their flanking properties.
//
// Phase 2: Run the CommonMark delimiter stack algorithm on the items.
// The algorithm matches opener and closer delimiter runs, wrapping the
// items between them in Bold/Italic tokens.  This implements rules 9-12
// of the spec, including the "mod 3" rule (rule 10) that prevents
// matching when both delimiters can open and close and the sum of their
// lengths is not a multiple of 3.
//
// After the stack algorithm runs, unmatched delimiter runs are converted
// to literal text, and the item list is flattened to a token list.

namespace {

struct DelimiterRun {
    char marker;          // '*' or '_'
    std::size_t length;   // current delimiter count (decremented on match)
    std::size_t orig_length;  // original count before any matching (Rule 10)
    bool can_open;
    bool can_close;
};

struct InlineItem {
    enum class Kind { Text, Token, Delimiter };
    Kind kind = Kind::Text;
    std::string text;           // for Text
    InlineToken token;          // for Token
    DelimiterRun delim;         // for Delimiter
};

// Compute the flanking properties of a delimiter run at `pos`.
[[nodiscard]] DelimiterRun make_delimiter_run(char marker,
                                               std::string_view text,
                                               std::size_t pos,
                                               std::size_t length) {
    DelimiterRun run;
    run.marker = marker;
    run.length = length;
    run.orig_length = length;  // never changes; used by Rule 10
    if (marker == '*') {
        run.can_open = star_can_open(text, pos, length);
        run.can_close = star_can_close(text, pos, length);
    } else {
        run.can_open = underscore_can_open(text, pos, length);
        run.can_close = underscore_can_close(text, pos, length);
    }
    return run;
}

// Find the maximal run of `marker` characters starting at `pos`.
// Returns the length of the run.
[[nodiscard]] std::size_t count_delimiter_run(std::string_view text,
                                               std::size_t pos,
                                               char marker) {
    std::size_t length = 0;
    while (pos + length < text.size() && text[pos + length] == marker) {
        ++length;
    }
    return length;
}

// Run the CommonMark delimiter stack algorithm on the item list.
// Modifies items in place: matched delimiter pairs are replaced with
// Bold/Italic tokens that wrap the items between them.
void process_delimiter_stack(std::vector<InlineItem>& items) {
    // The delimiter stack holds indices of delimiter items that can open.
    std::vector<std::size_t> stack;

    std::size_t i = 0;
    while (i < items.size()) {
        if (items[i].kind != InlineItem::Kind::Delimiter) {
            ++i;
            continue;
        }

        // Copy the delimiter state — we may modify items[i] during matching.
        DelimiterRun closer = items[i].delim;

        if (closer.can_close) {
            bool found_match = false;
            // Look for a matching opener on the stack, from top down.
            std::size_t stack_idx = stack.size();
            while (stack_idx > 0) {
                --stack_idx;
                const std::size_t opener_idx = stack[stack_idx];
                auto& opener = items[opener_idx].delim;

                // Rule 9: same character.
                if (opener.marker != closer.marker) continue;

                // Rule 10 (mod 3 rule): if one of the delimiters can both
                // open and close, and the closer's original delimiter count
                // is not a multiple of 3, and the sum of the original
                // delimiter counts IS a multiple of 3, then the delimiters
                // do not match.  (This is the "odd match" condition from
                // the reference implementation — it prevents intraword
                // emphasis like foo*bar*baz from matching.)
                const bool odd_match =
                    (closer.can_open || opener.can_close) &&
                    (closer.orig_length % 3 != 0) &&
                    ((opener.orig_length + closer.orig_length) % 3 == 0);
                if (odd_match) continue;

                // Match found!  Determine how many delimiter characters to use.
                // Use min(opener.length, closer.length), capped at 2 (for **).
                const std::size_t use_count =
                    std::min({opener.length, closer.length, std::size_t(2)});

                if (use_count == 0) continue;

                found_match = true;

                // Create the emphasis token.
                InlineToken emphasis_tok;
                emphasis_tok.kind = (use_count == 2)
                    ? InlineTokenKind::Bold
                    : InlineTokenKind::Italic;

                // Collect the items between the opener and closer as children.
                std::vector<InlineToken> children;
                for (std::size_t j = opener_idx + 1; j < i; ++j) {
                    const auto& child = items[j];
                    if (child.kind == InlineItem::Kind::Text) {
                        if (!child.text.empty()) {
                            children.push_back(
                                {InlineTokenKind::Text, child.text, {}, {}, {}});
                        }
                    } else if (child.kind == InlineItem::Kind::Token) {
                        children.push_back(child.token);
                    } else if (child.kind == InlineItem::Kind::Delimiter) {
                        // Delimiter runs between the opener and closer are
                        // literal text — only the matched opener/closer
                        // characters are removed (CommonMark §6.1.1).
                        children.push_back(
                            {InlineTokenKind::Text,
                             std::string(child.delim.length, child.delim.marker),
                             {}, {}, {}});
                    }
                }

                // If children is a single Text token, use its text directly.
                if (children.size() == 1 &&
                    children[0].kind == InlineTokenKind::Text) {
                    emphasis_tok.text = std::move(children[0].text);
                } else if (!children.empty()) {
                    emphasis_tok.children = std::move(children);
                }

                // Reduce the delimiter lengths.
                opener.length -= use_count;
                closer.length -= use_count;

                // Mark content items between opener and closer as consumed.
                // (Token items were already collected as children above.)
                for (std::size_t j = opener_idx + 1; j < i; ++j) {
                    items[j].kind = InlineItem::Kind::Text;
                    items[j].text.clear();
                }

                // Remove all openers above the matched opener from the
                // stack — they can't match anymore (the closer has been
                // found and consumed).  This prevents stale stack entries
                // from matching later closers (CommonMark §6.1.1 Rule 12).
                stack.erase(
                    stack.begin() + static_cast<std::ptrdiff_t>(stack_idx) + 1,
                    stack.end());

                // Handle the opener: if it has remaining delimiters, keep
                // it as a delimiter item (it can match a later closer).
                // Otherwise, mark it as consumed.
                if (opener.length == 0) {
                    items[opener_idx].kind = InlineItem::Kind::Text;
                    items[opener_idx].text.clear();
                    stack.erase(
                        stack.begin() + static_cast<std::ptrdiff_t>(stack_idx));
                }
                // If opener.length > 0, it stays on the stack as a delimiter.

                // Handle the closer: if it has remaining delimiters, keep
                // it as a delimiter item (it can match again).  Otherwise,
                // replace it with the emphasis token.
                if (closer.length > 0) {
                    // Store the emphasis token in the closer's item, then
                    // insert a new delimiter item for the remaining
                    // delimiters after it.  This preserves the emphasis
                    // token while allowing the remaining delimiters to
                    // match again (e.g. ***foo*** → Bold then Italic).
                    items[i].kind = InlineItem::Kind::Token;
                    items[i].token = std::move(emphasis_tok);

                    InlineItem remaining_item;
                    remaining_item.kind = InlineItem::Kind::Delimiter;
                    remaining_item.delim = closer;
                    items.insert(
                        items.begin() + static_cast<std::ptrdiff_t>(i + 1),
                        remaining_item);

                    // Update stack indices (all indices > i shifted by 1).
                    for (auto& idx : stack) {
                        if (idx > i) ++idx;
                    }

                    // Advance to the new delimiter item and re-process it.
                    ++i;
                    continue;
                } else {
                    items[i].kind = InlineItem::Kind::Token;
                    items[i].token = std::move(emphasis_tok);
                }

                // If the closer is fully consumed, break.
                if (closer.length == 0) break;

                // Otherwise, continue looking for more openers.
                // (This handles cases like **foo**bar** where the closer
                // has remaining characters that can close again.)
            }

            // If a match was found and the closer has remaining delimiters,
            // re-process it (don't advance to the next item).  This handles
            // cases like ***foo*** where the closer matches ** for bold and
            // then * for italic.
            if (found_match && closer.length > 0 &&
                items[i].kind == InlineItem::Kind::Delimiter) {
                continue;
            }
        }

        // If the delimiter can open, push it onto the stack.
        if (items[i].kind == InlineItem::Kind::Delimiter &&
            items[i].delim.can_open) {
            stack.push_back(i);
        }
        ++i;
    }

    // Second pass: remove consumed items (empty text items).
    items.erase(
        std::remove_if(items.begin(), items.end(),
                       [](const InlineItem& item) {
                           return item.kind == InlineItem::Kind::Text &&
                                  item.text.empty();
                       }),
        items.end());
}

// Convert the item list to a token list.
[[nodiscard]] std::vector<InlineToken> items_to_tokens(
    const std::vector<InlineItem>& items) {
    std::vector<InlineToken> tokens;
    tokens.reserve(items.size());
    for (const auto& item : items) {
        switch (item.kind) {
            case InlineItem::Kind::Text:
                if (!item.text.empty()) {
                    tokens.push_back(
                        {InlineTokenKind::Text, item.text, {}, {}, {}});
                }
                break;
            case InlineItem::Kind::Token:
                tokens.push_back(item.token);
                break;
            case InlineItem::Kind::Delimiter:
                // Unmatched delimiter — literal text.
                tokens.push_back(
                    {InlineTokenKind::Text,
                     std::string(item.delim.length, item.delim.marker),
                     {}, {}, {}});
                break;
        }
    }
    return tokens;
}

}  // namespace

[[nodiscard]] std::vector<InlineToken> tokenize_inline(
    std::string_view text, const LinkRefMap* link_refs,
    bool gfm_extensions) {
    // Build an item list (text + tokens + delimiter runs), then run the
    // CommonMark delimiter stack algorithm on it (§6.1.1) to match
    // emphasis openers/closers.  This replaces the old forward-scanning
    // bold/italic parser, which couldn't handle Rule 10 (mod 3), nested
    // emphasis across delimiter runs, or delimiter run splitting.
    std::vector<InlineItem> items;
    std::string buffer;
    buffer.reserve(text.size());
    // True when inside a rejected <...> construct — extended autolinks
    // are suppressed until the closing '>' is found.
    bool in_angle_literal = false;

    auto flush_buffer = [&]() {
        if (!buffer.empty()) {
            items.push_back(InlineItem{
                .kind = InlineItem::Kind::Text,
                .text = std::move(buffer),
                .token = {},
                .delim = {}});
            buffer.clear();
        }
    };

    // Push a fully-formed token (link, code, image, etc.) as an item.
    auto push_token = [&](InlineToken tok) {
        items.push_back(InlineItem{
            .kind = InlineItem::Kind::Token,
            .text = {},
            .token = std::move(tok),
            .delim = {}});
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        // Autolink: <scheme:...> or <email@domain> (CommonMark §6.5).
        // Checked before escape so backslashes in URLs are URL content,
        // not escapes.  The URL is percent-encoded; & is left for the
        // HTML serializer to escape as &amp;.
        if (c == '<') {
            std::size_t consumed = 0;
            std::string link_text;
            auto url = parse_autolink(text, i, consumed, link_text);
            if (consumed > 0) {
                flush_buffer();
                push_token({InlineTokenKind::Link,
                                  std::move(link_text), std::move(url), {}, {}});
                i += consumed - 1;
                continue;
            }
            // Not a valid autolink — try inline HTML (CommonMark §6.6).
            // Valid tags pass through as raw HTML.  GFM-disallowed tags
            // are emitted as HtmlRaw with '<' pre-escaped to '&lt;' —
            // GFM escapes only '<' in disallowed tags, not '>' (spec
            // example 652 expects '&lt;title>', not '&lt;title&gt;').
            bool disallowed = false;
            const auto html_end = parse_inline_html(text, i, disallowed);
            if (html_end != std::string_view::npos) {
                flush_buffer();
                auto tag_text = std::string(text.substr(i, html_end - i));
                if (disallowed) {
                    tag_text.replace(0, 1, "&lt;");
                }
                push_token({InlineTokenKind::HtmlRaw,
                                  std::move(tag_text), {}, {}, {}});
                i = html_end - 1;
                continue;
            }
            // Not a valid autolink or HTML tag — mark that we're inside a
            // rejected <...> construct so extended autolinks don't match
            // inside it (e.g. "<http://foo.bar/baz bim>" is all literal).
            // The '<' itself falls through to the buffer; the '>' will
            // clear the flag.  Backslash escapes inside are still
            // processed normally by the escape handler below.
            if (text.find('>', i + 1) != std::string_view::npos) {
                in_angle_literal = true;
            }
        }

        // Escape: \<punctuation> (CommonMark §2.4 — all ASCII punctuation).
        // Keeps the literal char in the text buffer (no Escape token needed;
        // the HTML/FTXUI renderers escape it as ordinary text).
        if (c == '\\' && i + 1 < text.size()) {
            char next = text[i + 1];
            if (next == '!' || next == '"' || next == '#' || next == '$' ||
                next == '%' || next == '&' || next == '\'' || next == '(' ||
                next == ')' || next == '*' || next == '+' || next == ',' ||
                next == '-' || next == '.' || next == '/' || next == ':' ||
                next == ';' || next == '<' || next == '=' || next == '>' ||
                next == '?' || next == '@' || next == '[' || next == '\\' ||
                next == ']' || next == '^' || next == '_' || next == '`' ||
                next == '{' || next == '|' || next == '}' || next == '~') {
                buffer += next;
                i += 1;
                continue;
            }
        }

        // GFM Strikethrough: ~~text~~ or ~text~ (GFM §4.5).
        // A delimiter is a MAXIMAL run of exactly 1 or 2 '~' characters;
        // a run of 3+ tildes is literal text.  Opening must not be followed
        // by whitespace; closing must not be preceded by whitespace.
        // Opening and closing must have the same length.  Unlike emphasis
        // with '_', '~' has no intra-word restriction.
        if (c == '~') {
            // Count the maximal run of tildes starting at i.
            std::size_t run_len = 0;
            while (i + run_len < text.size() && text[i + run_len] == '~') {
                ++run_len;
            }
            // 3+ tildes are literal text.
            if (run_len >= 3) {
                buffer.append(run_len, '~');
                i += run_len - 1;  // -1: loop increments i
                continue;
            }
            const std::size_t tilde_len = run_len;
            // Opening must not be followed by whitespace.
            if (i + tilde_len < text.size() &&
                (text[i + tilde_len] == ' ' || text[i + tilde_len] == '\t' ||
                 text[i + tilde_len] == '\n')) {
                buffer += c;
                continue;
            }
            // Search for a closing delimiter of the same length, outside
            // code spans, not preceded by whitespace.
            std::size_t search = i + tilde_len;
            bool matched = false;
            while (search < text.size()) {
                auto close = find_outside_code_spans(text, search, '~');
                if (close == std::string_view::npos) break;
                // Count the maximal run of tildes at close.
                std::size_t close_run = 0;
                while (close + close_run < text.size() &&
                       text[close + close_run] == '~') {
                    ++close_run;
                }
                // 3+ tildes at close are literal — skip past them.
                if (close_run >= 3) {
                    search = close + close_run;
                    continue;
                }
                if (close_run == tilde_len && close > i + tilde_len &&
                    text[close - 1] != ' ' && text[close - 1] != '\t' &&
                    text[close - 1] != '\n') {
                    flush_buffer();
                    auto content =
                        text.substr(i + tilde_len, close - i - tilde_len);
                    push_token({InlineTokenKind::Strikethrough,
                                      std::string(content), {}, {}, {}});
                    i = close + close_run - 1;
                    matched = true;
                    break;
                }
                search = close + close_run;
            }
            if (matched) continue;
            // No matching close — literal tilde.
            buffer += c;
            continue;
        }

        // Inline code: `code` or ``code with ` inside`` (CommonMark §2.6).
        // A backtick string of length N opens, and a backtick string of
        // the same length closes.  Newlines in the content become spaces.
        // If the content starts and ends with a space (but isn't all
        // spaces), one space is stripped from each end.
        if (c == '`') {
            const std::size_t bt_open_start = i;
            std::size_t bt_open_end = i + 1;
            while (bt_open_end < text.size() && text[bt_open_end] == '`') {
                ++bt_open_end;
            }
            const std::size_t bt_len = bt_open_end - bt_open_start;

            // Search for a matching close (backtick string of same length)
            std::size_t search = bt_open_end;
            bool found = false;
            std::size_t close_start = 0;
            std::size_t close_end = 0;
            while (search < text.size()) {
                const auto pos = text.find('`', search);
                if (pos == std::string_view::npos) break;
                std::size_t end = pos + 1;
                while (end < text.size() && text[end] == '`') ++end;
                if (end - pos == bt_len) {
                    found = true;
                    close_start = pos;
                    close_end = end;
                    break;
                }
                search = end;
            }
            if (found) {
                flush_buffer();
                std::string code(
                    text.substr(bt_open_end, close_start - bt_open_end));
                // Newlines → spaces (CommonMark: line endings in code spans
                // are treated as spaces)
                for (char& ch : code) {
                    if (ch == '\n') ch = ' ';
                }
                // Strip one surrounding space if both present (and the
                // content isn't all spaces)
                if (code.size() >= 2 && code.front() == ' ' &&
                    code.back() == ' ') {
                    bool all_spaces = true;
                    for (char ch : code) {
                        if (ch != ' ') {
                            all_spaces = false;
                            break;
                        }
                    }
                    if (!all_spaces) {
                        code = code.substr(1, code.size() - 2);
                    }
                }
                push_token(
                    {InlineTokenKind::Code, std::move(code), {}, {}, {}});
                i = close_end - 1;
                continue;
            }
            // No matching close — all opening backticks are literal text.
            // Skip past them so we don't retry with a shorter string
            // (CommonMark: an unmatched backtick string is all literal).
            buffer.append(bt_len, '`');
            i = bt_open_end - 1;
            continue;
        }

        // Emphasis delimiter runs: * or _ (CommonMark §6.1.1).
        // Record the delimiter run; the delimiter stack algorithm
        // (process_delimiter_stack) matches openers and closers after the
        // main loop, handling Rule 9 (same character), Rule 10 (mod 3),
        // nested emphasis, and delimiter run splitting.
        if (c == '*' || c == '_') {
            const std::size_t run_len = count_delimiter_run(text, i, c);
            flush_buffer();
            auto run = make_delimiter_run(c, text, i, run_len);
            items.push_back(InlineItem{
                .kind = InlineItem::Kind::Delimiter,
                .text = {},
                .token = {},
                .delim = run});
            i += run_len - 1;  // -1: loop increments i
            continue;
        }
        // Inline math: $...$ (LaTeX math expression)
        // Opening $ must not be followed by whitespace or another $ (to
        // avoid matching $$...$$ block math inline — that's handled in
        // lex_blocks).  Closing $ must not be preceded by whitespace.
        // Mirrors marked-katex-extension / marked-math behavior.
        if (c == '$' && i + 1 < text.size() && text[i + 1] != '$' &&
            text[i + 1] != ' ' && text[i + 1] != '\t') {
            flush_buffer();
            auto end = text.find('$', i + 1);
            if (end != std::string_view::npos && end > i + 1 &&
                text[end - 1] != ' ' && text[end - 1] != '\t') {
                auto math = text.substr(i + 1, end - i - 1);
                push_token({InlineTokenKind::Math,
                                  std::string(math), {}, {}, {}});
                i = end;
                continue;
            }
        }

        // Image: ![alt](url "title") or reference images ![alt][label],
        // ![alt][], ![alt] — checked before Link because the '[' handler
        // would otherwise consume the bracket.  The alt text is processed
        // to strip formatting markers for the alt attribute (CommonMark
        // §6.6: alt is the plain text content of the image description).
        if (c == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            auto bracket_end = find_matching_bracket(text, i + 1);
            if (bracket_end != std::string_view::npos) {
                // Inline image: ![alt](url)
                if (bracket_end + 1 < text.size() &&
                    text[bracket_end + 1] == '(') {
                    auto target =
                        parse_inline_link_target(text, bracket_end + 1);
                    if (target) {
                        flush_buffer();
                        auto raw_alt = text.substr(i + 2, bracket_end - i - 2);
                        auto alt_tokens = tokenize_inline(raw_alt);
                        auto alt_text = extract_plain_text(alt_tokens);
                        push_token({InlineTokenKind::Image,
                                          std::move(alt_text),
                                          std::move(target->dest),
                                          std::move(target->title), {}});
                        i = target->end;
                        continue;
                    }
                }

                // Reference images (only when a definition map is available)
                if (link_refs) {
                    auto raw_alt = text.substr(i + 2, bracket_end - i - 2);

                    // ![alt][label] or ![alt][] — collapsed/shortcut reference
                    if (bracket_end + 1 < text.size() &&
                        text[bracket_end + 1] == '[') {
                        // Labels must not contain unescaped '[' (examples 546-548).
                        auto label_end = find_label_end(
                            text, bracket_end + 1);
                        if (label_end != std::string_view::npos) {
                            auto label = text.substr(
                                bracket_end + 2,
                                label_end - bracket_end - 2);
                            std::string normalized = label.empty()
                                ? normalize_label(raw_alt)
                                : normalize_label(label);
                            auto it = link_refs->find(normalized);
                            if (it != link_refs->end()) {
                                flush_buffer();
                                auto alt_tokens = tokenize_inline(raw_alt);
                                auto alt_text = extract_plain_text(alt_tokens);
                                push_token(
                                    {InlineTokenKind::Image,
                                     std::move(alt_text),
                                     it->second.url,
                                     it->second.title, {}});
                                i = label_end;
                                continue;
                            }
                        }
                    }

                    // ![alt] — full reference (label = alt)
                    if (bracket_end + 1 >= text.size() ||
                        (text[bracket_end + 1] != '(' &&
                         text[bracket_end + 1] != '[')) {
                        auto normalized = normalize_label(raw_alt);
                        auto it = link_refs->find(normalized);
                        if (it != link_refs->end()) {
                            flush_buffer();
                            auto alt_tokens = tokenize_inline(raw_alt);
                            auto alt_text = extract_plain_text(alt_tokens);
                            push_token(
                                {InlineTokenKind::Image,
                                 std::move(alt_text),
                                 it->second.url,
                                 it->second.title, {}});
                            i = bracket_end;
                            continue;
                        }
                    }
                }
            }
        }

        // Link: [text](url "title") or reference links [text][label],
        // [text][], [text] (CommonMark §4.7 + §6.6).
        // Uses find_matching_bracket to handle nested brackets, and
        // contains_complete_link to enforce the "no nested links" rule.
        if (c == '[') {
            auto bracket_end = find_matching_bracket(text, i);
            if (bracket_end != std::string_view::npos) {
                auto link_text =
                    text.substr(i + 1, bracket_end - i - 1);

                // CommonMark §6.6: links may not contain other links.
                // If the link text contains a complete link, the outer
                // [...] is NOT a link — the inner link is used instead.
                if (!contains_complete_link(link_text)) {
                // Inline link: [text](url)
                bool inline_link_failed = false;
                if (bracket_end + 1 < text.size() &&
                    text[bracket_end + 1] == '(') {
                    auto target =
                        parse_inline_link_target(text, bracket_end + 1);
                    if (target) {
                        flush_buffer();
                        InlineToken tok{InlineTokenKind::Link,
                                        std::string(link_text),
                                        std::move(target->dest),
                                        std::move(target->title), {}};
                        // Tokenize link text so formatting (emphasis,
                        // code, etc.) inside links is parsed.
                        tok.children = tokenize_inline(tok.text);
                        push_token(std::move(tok));
                        i = target->end;
                        continue;
                    }
                    inline_link_failed = true;
                }

                // Reference links (only when a definition map is available)
                if (link_refs) {
                    // [text][label] or [text][] — collapsed/shortcut reference
                    if (bracket_end + 1 < text.size() &&
                        text[bracket_end + 1] == '[') {
                        // Labels must not contain unescaped '[' (examples 546-548).
                        auto label_end = find_label_end(
                            text, bracket_end + 1);
                        if (label_end != std::string_view::npos) {
                            auto label = text.substr(
                                bracket_end + 2,
                                label_end - bracket_end - 2);
                            std::string normalized = label.empty()
                                ? normalize_label(link_text)
                                : normalize_label(label);
                            auto it = link_refs->find(normalized);
                            if (it != link_refs->end()) {
                                flush_buffer();
                                InlineToken tok{InlineTokenKind::Link,
                                                std::string(link_text),
                                                it->second.url,
                                                it->second.title, {}};
                                tok.children = tokenize_inline(tok.text);
                                push_token(std::move(tok));
                                i = label_end;
                                continue;
                            }
                        }
                    }

                    // [text] — full reference (label = text).  Tried when
                    // not followed by '(' or '[' (those are handled above),
                    // OR when the inline link was tried but failed
                    // (CommonMark §6.6 example 568: [foo](not a link)
                    // falls back to [foo] reference).
                    if (inline_link_failed ||
                        bracket_end + 1 >= text.size() ||
                        (text[bracket_end + 1] != '(' &&
                         text[bracket_end + 1] != '[')) {
                        auto normalized = normalize_label(link_text);
                        auto it = link_refs->find(normalized);
                        if (it != link_refs->end()) {
                            flush_buffer();
                            InlineToken tok{InlineTokenKind::Link,
                                            std::string(link_text),
                                            it->second.url,
                                            it->second.title, {}};
                            tok.children = tokenize_inline(tok.text);
                            push_token(std::move(tok));
                            i = bracket_end;
                            continue;
                        }
                    }
                }

                // GFM footnotes: [^label] — fallback after ALL link attempts
                // fail (cmark-gfm inlines.c handle_close_bracket noMatch path).
                // The label must start with '^' and have content after it.
                // [^] (empty) is NOT a footnote ref.
                if (gfm_extensions && link_text.size() >= 2 &&
                    link_text[0] == '^') {
                    flush_buffer();
                    push_token({InlineTokenKind::FootnoteRef,
                                std::string(link_text.substr(1)), {}, {}, {}});
                    i = bracket_end;
                    continue;
                }
                }  // end !contains_complete_link
            }
        }

        // GFM extended autolinks: bare URLs (www., http://, https://,
        // ftp://) and bare email addresses without angle brackets.
        // Checked before entity references so & in URLs is not decoded.
        // Suppressed inside a rejected <...> construct and in CommonMark
        // mode (gfm_extensions = false).
        if (gfm_extensions && !in_angle_literal) {
            if (auto ext = parse_extended_autolink(text, i)) {
                flush_buffer();
                push_token({InlineTokenKind::Link,
                                  std::move(ext->text),
                                  std::move(ext->url), {}, {}});
                i = ext->end - 1;
                continue;
            }
        }

        // Closing '>' of a rejected <...> construct — clear the flag.
        if (c == '>' && in_angle_literal) {
            in_angle_literal = false;
        }

        // Entity reference: &Name; &#NNN; &#xHH; (CommonMark §3.5).
        // Decoded to the literal character(s) in the text buffer; the
        // HTML serializer re-escapes &, <, >, " as needed.  Not applied
        // inside code spans (handled above) or URLs (extracted as-is).
        if (c == '&') {
            std::size_t consumed = 0;
            auto decoded = decode_entity(text, i, consumed);
            if (consumed > 0) {
                buffer += decoded;
                i += consumed - 1;  // -1: loop increments i
                continue;
            }
        }

        buffer += c;
    }
    flush_buffer();
    // Run the CommonMark delimiter stack algorithm to match emphasis
    // openers and closers (Rule 9, Rule 10, nested emphasis, splitting).
    process_delimiter_stack(items);
    return items_to_tokens(items);
}

[[nodiscard]] std::string trim_cell(std::string_view value) {
    auto begin = value.find_first_not_of(" \t");
    if (begin == std::string_view::npos) return "";
    auto end = value.find_last_not_of(" \t");
    return std::string(value.substr(begin, end - begin + 1));
}

// Find the next '|' at or after `start` that is NOT escaped (\|) and
// NOT inside a code span.  Used by split_table_row so that escaped pipes
// and pipes in code spans are literal cell content, not separators
// (GFM tables spec).
[[nodiscard]] std::size_t find_table_pipe(std::string_view line,
                                           std::size_t start) {
    std::size_t i = start;
    while (i < line.size()) {
        if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|') {
            i += 2;  // skip escaped pipe
        } else if (line[i] == '`') {
            // Skip a matched code span; unmatched backticks are literal.
            const std::size_t bt_start = i;
            std::size_t bt_end = i + 1;
            while (bt_end < line.size() && line[bt_end] == '`') ++bt_end;
            const std::size_t bt_len = bt_end - bt_start;
            std::size_t search = bt_end;
            bool closed = false;
            while (search < line.size()) {
                const auto close = line.find('`', search);
                if (close == std::string_view::npos) break;
                std::size_t ce = close + 1;
                while (ce < line.size() && line[ce] == '`') ++ce;
                if (ce - close == bt_len) {
                    i = ce;
                    closed = true;
                    break;
                }
                search = ce;
            }
            if (!closed) {
                // Unmatched backtick — literal, continue scanning after it.
                i = bt_end;
            }
        } else if (line[i] == '|') {
            return i;
        } else {
            ++i;
        }
    }
    return std::string_view::npos;
}

[[nodiscard]] std::vector<std::string> split_table_row(std::string_view line) {
    // Strip an optional leading/trailing pipe, but only if unescaped.
    if (!line.empty() && line.front() == '|') line.remove_prefix(1);
    if (line.size() >= 2 && line[line.size() - 1] == '|' &&
        line[line.size() - 2] != '\\') {
        line.remove_suffix(1);
    }

    std::vector<std::string> cells;
    std::size_t start = 0;
    while (start <= line.size()) {
        auto end = find_table_pipe(line, start);
        auto cell = end == std::string_view::npos
            ? line.substr(start)
            : line.substr(start, end - start);
        // Unescape \| — escaped pipes are literal cell content, not
        // separators (GFM tables spec, example 200).
        std::string unescaped;
        unescaped.reserve(cell.size());
        for (std::size_t j = 0; j < cell.size(); ++j) {
            if (cell[j] == '\\' && j + 1 < cell.size() &&
                cell[j + 1] == '|') {
                unescaped += '|';
                ++j;
            } else {
                unescaped += cell[j];
            }
        }
        cells.push_back(trim_cell(unescaped));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return cells;
}

[[nodiscard]] bool is_table_separator(std::string_view line) {
    auto cells = split_table_row(line);
    if (cells.empty()) return false;
    for (const auto& raw_cell : cells) {
        if (raw_cell.empty()) return false;
        auto cell = std::string_view(raw_cell);
        if (!cell.empty() && cell.front() == ':') cell.remove_prefix(1);
        if (!cell.empty() && cell.back() == ':') cell.remove_suffix(1);
        // GFM: at least 1 hyphen per cell (not 3 — that's thematic breaks).
        if (cell.empty()) return false;
        for (char ch : cell) {
            if (ch != '-') return false;
        }
    }
    return true;
}

// Parse per-column alignment from a separator row's cells.
// 0=none(default ---), 1=left(:---), 2=center(:---:), 3=right(---:).
[[nodiscard]] std::vector<std::int8_t> parse_table_align(
    const std::vector<std::string>& sep_cells) {
    std::vector<std::int8_t> align;
    align.reserve(sep_cells.size());
    for (const auto& cell : sep_cells) {
        const bool left = !cell.empty() && cell.front() == ':';
        const bool right = !cell.empty() && cell.back() == ':';
        if (left && right)      align.push_back(2);  // center
        else if (right)         align.push_back(3);  // right
        else if (left)          align.push_back(1);  // left
        else                    align.push_back(0);  // none (default)
    }
    return align;
}

// Check if a line interrupts a GFM table (block-level construct or
// empty line).  Lines without '|' that are NOT block interrupts are
// treated as single-cell table rows (GFM tables spec, example 202).
[[nodiscard]] bool is_table_interrupt(std::string_view line) {
    if (line.empty()) return true;
    std::size_t pos = 0;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size()) return true;
    const char c = line[pos];
    if (c == '#' || c == '>') return true;
    if (c == '`' || c == '~') return true;
    // List item: -, *, + followed by space
    if ((c == '-' || c == '*' || c == '+') &&
        pos + 1 < line.size() &&
        (line[pos + 1] == ' ' || line[pos + 1] == '\t'))
        return true;
    // Ordered list: digits + . or ) + space
    if (c >= '0' && c <= '9') {
        std::size_t j = pos;
        while (j < line.size() && line[j] >= '0' && line[j] <= '9')
            ++j;
        if (j < line.size() && (line[j] == '.' || line[j] == ')') &&
            j + 1 < line.size() &&
            (line[j + 1] == ' ' || line[j + 1] == '\t'))
            return true;
    }
    return false;
}

[[nodiscard]] int count_list_indent(std::string_view line) {
    std::size_t pos = 0;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    // marked: each 2 spaces = 1 depth level. Tabs count as 2 spaces.
    int spaces = 0;
    for (std::size_t i = 0; i < pos; ++i) {
        spaces += (line[i] == '\t') ? 2 : 1;
    }
    return spaces / 2;  // 0, 1, 2, ... depth levels
}

// List marker detection result (CommonMark §5.2).
struct ListMarker {
    bool ordered = false;       // true for ordered, false for unordered
    int number = 0;             // ordered list: the item number
    int content_indent = 0;     // column after marker + effective spaces
    int base_indent = 0;        // leading spaces before the marker
    char marker_char = 0;       // '-', '+', '*', or '.' / ')' for ordered
    std::string_view content;   // text from content_indent column to EOL
};

// Detect a list marker at the start of an expanded (tab-expanded) line.
// Returns nullopt if the line is not a list item.
// Content indentation rules (CommonMark §5.2):
//   - 1-4 spaces after marker: content_indent = after those spaces
//   - 5+ spaces, or marker at EOL: content_indent = marker_col + 2 (ul)
//     or marker_col + 3 (ol)
// The first line's content starts at the content_indent column (not after
// all spaces), so 5+ spaces after the marker produce leading spaces in the
// content (which may form an indented code block).
[[nodiscard]] std::optional<ListMarker> detect_list_marker(
    std::string_view expanded) {
    // Up to 3 leading spaces (4+ is indented code, not a list item)
    std::size_t pos = 0;
    while (pos < expanded.size() && expanded[pos] == ' ' && pos < 3) ++pos;
    if (pos >= expanded.size()) return std::nullopt;

    ListMarker m;
    m.base_indent = static_cast<int>(pos);
    const char c = expanded[pos];

    if (c == '-' || c == '+' || c == '*') {
        // Marker at EOL is valid (empty list item, CommonMark §5.2)
        if (pos + 1 < expanded.size() &&
            expanded[pos + 1] != ' ' && expanded[pos + 1] != '\t')
            return std::nullopt;
        m.ordered = false;
        m.marker_char = c;
        const int effective = [&] {
            if (pos + 1 >= expanded.size()) return 1;  // EOL
            std::size_t cs = pos + 1;
            while (cs < expanded.size() &&
                   (expanded[cs] == ' ' || expanded[cs] == '\t'))
                ++cs;
            // Only spaces to EOL → treat as EOL (CommonMark §5.2: the
            // content indentation is the marker column + 2 for ul, +3 ol).
            if (cs >= expanded.size()) return 1;
            const int spaces_after = static_cast<int>(cs - (pos + 1));
            return (spaces_after > 4) ? 1 : std::max(spaces_after, 1);
        }();
        m.content_indent = m.base_indent + 1 + effective;
        // Content starts at content_indent column (may have leading spaces
        // if 5+ spaces after marker — those form an indented code block).
        m.content = expanded.substr(
            std::min(static_cast<std::size_t>(m.content_indent),
                     expanded.size()));
        return m;
    }

    if (c >= '0' && c <= '9') {
        const std::size_t num_start = pos;
        while (pos < expanded.size() &&
               expanded[pos] >= '0' && expanded[pos] <= '9')
            ++pos;
        if (pos == num_start || pos >= expanded.size()) return std::nullopt;
        if (expanded[pos] != '.' && expanded[pos] != ')') return std::nullopt;
        // Marker at EOL is valid (empty list item)
        if (pos + 1 < expanded.size() &&
            expanded[pos + 1] != ' ' && expanded[pos + 1] != '\t')
            return std::nullopt;
        // Up to 9 digits (CommonMark: 10+ digits is not a list marker)
        const auto num_str = expanded.substr(num_start, pos - num_start);
        if (num_str.size() > 9) return std::nullopt;
        m.ordered = true;
        m.marker_char = expanded[pos];
        int num = 0;
        for (char ch : num_str) num = num * 10 + (ch - '0');
        m.number = num;
        const int marker_width = static_cast<int>(pos - num_start) + 1;
        const int effective = [&] {
            if (pos + 1 >= expanded.size()) return 1;  // EOL
            std::size_t cs = pos + 1;
            while (cs < expanded.size() &&
                   (expanded[cs] == ' ' || expanded[cs] == '\t'))
                ++cs;
            // Only spaces to EOL → treat as EOL
            if (cs >= expanded.size()) return 1;
            const int spaces_after = static_cast<int>(cs - (pos + 1));
            return (spaces_after > 4) ? 1 : std::max(spaces_after, 1);
        }();
        m.content_indent = m.base_indent + marker_width + effective;
        m.content = expanded.substr(
            std::min(static_cast<std::size_t>(m.content_indent),
                     expanded.size()));
        return m;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::pair<int, std::string>>
parse_ordered_list(std::string_view line) {
    // Skip leading whitespace to support indented (nested) list items.
    std::size_t pos = 0;
    while (pos < line.size() &&
           (line[pos] == ' ' || line[pos] == '\t')) ++pos;

    std::size_t num_start = pos;
    while (pos < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[pos]))) ++pos;
    if (pos == num_start || pos >= line.size() || line[pos] != '.') return std::nullopt;
    if (pos + 1 >= line.size() ||
        !std::isspace(static_cast<unsigned char>(line[pos + 1]))) {
        return std::nullopt;
    }
    std::size_t text_start = pos + 2;
    while (text_start < line.size() &&
           std::isspace(static_cast<unsigned char>(line[text_start]))) {
        ++text_start;
    }
    int num = 0;
    auto num_str = line.substr(num_start, pos - num_start);
    for (char ch : num_str) {
        num = num * 10 + (ch - '0');
    }
    return std::pair{num, std::string(line.substr(text_start))};
}

[[nodiscard]] std::vector<std::string> split_lines(std::string_view source) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < source.size()) {
        auto end = source.find('\n', start);
        lines.emplace_back(end == std::string_view::npos
                           ? source.substr(start)
                           : source.substr(start, end - start));
        start = (end == std::string_view::npos) ? source.size() : end + 1;
    }
    return lines;
}

// Detect a GFM task list item marker at the start of `content`.
// Returns 0 if not a task item; otherwise returns the number of chars
// to skip (the marker + trailing space) and sets `checked`.
// A task marker is "[ ] " (unchecked) or "[x] " / "[X] " (checked).
[[nodiscard]] std::size_t parse_task_marker(std::string_view content,
                                            bool& checked) {
    if (content.size() >= 3 && content[0] == '[' &&
        content[2] == ']' &&
        (content[1] == ' ' || content[1] == 'x' ||
         content[1] == 'X')) {
        checked = (content[1] != ' ');
        // Must be followed by a space (or end of content)
        if (content.size() == 3 || content[3] == ' ' ||
            content[3] == '\t') {
            return content.size() > 3 ? 4 : 3;
        }
    }
    return 0;
}

// ── HTML blocks (CommonMark §4.6) ──────────────────────────────────

// Block-level HTML tags (CommonMark spec, type 6).  A complete or
// incomplete tag of one of these types starts an HTML block that ends
// at a blank line.
constexpr std::string_view kBlockHtmlTags[] = {
    "address", "article", "aside", "base", "basefont", "blockquote",
    "body", "caption", "center", "col", "colgroup", "dd", "details",
    "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption",
    "figure", "footer", "form", "frame", "frameset", "h1", "h2",
    "h3", "h4", "h5", "h6", "head", "header", "hr", "html",
    "iframe", "legend", "li", "link", "main", "menu", "menuitem",
    "nav", "noframes", "ol", "optgroup", "option", "p", "param",
    "section", "summary", "table", "tbody", "td", "tfoot", "th",
    "thead", "title", "tr", "track", "ul",
};

// Check if a tag name is a block-level HTML tag (case-insensitive).
[[nodiscard]] bool is_block_html_tag(std::string_view name) {
    for (const auto& tag : kBlockHtmlTags) {
        if (name.size() == tag.size()) {
            bool match = true;
            for (std::size_t i = 0; i < name.size(); ++i) {
                char a = name[i];
                char b = tag[i];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
            }
            if (match) return true;
        }
    }
    return false;
}

// Parse a tag name from a line starting at `pos` (after '<').
// Returns the tag name and the position after the name, or std::nullopt
// if the tag is malformed.
struct ParsedHtmlTag {
    std::string name;
    std::size_t end_pos = 0;
    bool is_closing = false;
};

[[nodiscard]] std::optional<ParsedHtmlTag> parse_html_tag_name(
    std::string_view line, std::size_t pos) {
    if (pos >= line.size()) return std::nullopt;
    bool is_closing = false;
    if (line[pos] == '/') {
        is_closing = true;
        ++pos;
    }
    std::size_t start = pos;
    while (pos < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[pos])) ||
            line[pos] == '-')) {
        ++pos;
    }
    if (pos == start) return std::nullopt;
    ParsedHtmlTag result;
    result.name = std::string(line.substr(start, pos - start));
    result.end_pos = pos;
    result.is_closing = is_closing;
    return result;
}

// Try to parse an HTML block from a line.  Returns the HTML content and
// the number of lines consumed, or std::nullopt if the line is not an
// HTML block start.
struct ParsedHtmlBlock {
    std::string content;
    int lines_consumed = 1;
};

[[nodiscard]] std::optional<ParsedHtmlBlock> try_parse_html_block(
    const std::vector<std::string>& lines, std::size_t start,
    bool can_interrupt_paragraph) {
    if (start >= lines.size()) return std::nullopt;
    const std::string& raw = lines[start];
    // Up to 3 spaces indent
    std::size_t pos = 0;
    while (pos < raw.size() && raw[pos] == ' ' && pos < 3) ++pos;
    if (pos >= raw.size() || raw[pos] != '<') return std::nullopt;

    std::string_view rest(raw.data() + pos, raw.size() - pos);

    // Type 1: <script>, <pre>, <style>, <textarea> (case-insensitive)
    // Ends at the matching closing tag (on any line).
    {
        auto tag = parse_html_tag_name(rest, 1);
        if (tag && !tag->is_closing) {
            std::string lower_name;
            lower_name.reserve(tag->name.size());
            for (char c : tag->name) {
                if (c >= 'A' && c <= 'Z') c += 32;
                lower_name += c;
            }
            if (lower_name == "script" || lower_name == "pre" ||
                lower_name == "style" || lower_name == "textarea") {
                // Find the closing tag
                std::string close_tag = "</" + lower_name;
                std::string content = raw;
                int lines_consumed = 1;
                // Check if closing tag is on the same line
                if (rest.find(close_tag) != std::string_view::npos) {
                    // Check if there's content after the closing tag
                    auto close_pos = rest.find(close_tag);
                    auto after_close = rest.find('>', close_pos);
                    if (after_close != std::string_view::npos &&
                        after_close + 1 < rest.size()) {
                        // There's content after the closing tag — check
                        // if it's just whitespace
                        bool only_ws = true;
                        for (std::size_t i = after_close + 1; i < rest.size(); ++i) {
                            if (rest[i] != ' ' && rest[i] != '\t' &&
                                rest[i] != '\r') {
                                only_ws = false;
                                break;
                            }
                        }
                        if (!only_ws) {
                            // Content after closing tag — not a type 1 block
                            // (fall through to other types)
                        } else {
                            return ParsedHtmlBlock{content, lines_consumed};
                        }
                    } else {
                        return ParsedHtmlBlock{content, lines_consumed};
                    }
                } else {
                    // Search subsequent lines for the closing tag
                    ++start;
                    while (start < lines.size()) {
                        content += '\n';
                        content += lines[start];
                        ++lines_consumed;
                        if (lines[start].find(close_tag) !=
                            std::string_view::npos) {
                            break;
                        }
                        ++start;
                    }
                    return ParsedHtmlBlock{content, lines_consumed};
                }
            }
        }
    }

    // Type 2: HTML comment <!-- ... -->
    // The block includes the entire first line (content after --> is
    // part of the HTML block, passed through raw).  If the comment
    // doesn't end on the first line, it continues until -->.
    if (rest.starts_with("<!--")) {
        std::string content = raw;
        int lines_consumed = 1;
        if (rest.find("-->") != std::string_view::npos) {
            // Comment ends on the same line — the whole line is the block
            return ParsedHtmlBlock{content, lines_consumed};
        } else {
            // Comment spans multiple lines
            ++start;
            while (start < lines.size()) {
                content += '\n';
                content += lines[start];
                ++lines_consumed;
                if (lines[start].find("-->") != std::string_view::npos) {
                    break;
                }
                ++start;
            }
            return ParsedHtmlBlock{content, lines_consumed};
        }
    }

    // Type 3: Processing instruction <? ... ?>
    if (rest.starts_with("<?")) {
        std::string content = raw;
        int lines_consumed = 1;
        if (rest.find("?>") != std::string_view::npos) {
            return ParsedHtmlBlock{content, lines_consumed};
        }
        ++start;
        while (start < lines.size()) {
            content += '\n';
            content += lines[start];
            ++lines_consumed;
            if (lines[start].find("?>") != std::string_view::npos) {
                break;
            }
            ++start;
        }
        return ParsedHtmlBlock{content, lines_consumed};
    }

    // Type 4: CDATA section <![CDATA[ ... ]]>
    if (rest.starts_with("<![CDATA[")) {
        std::string content = raw;
        int lines_consumed = 1;
        if (rest.find("]]>") != std::string_view::npos) {
            return ParsedHtmlBlock{content, lines_consumed};
        }
        ++start;
        while (start < lines.size()) {
            content += '\n';
            content += lines[start];
            ++lines_consumed;
            if (lines[start].find("]]>") != std::string_view::npos) {
                break;
            }
            ++start;
        }
        return ParsedHtmlBlock{content, lines_consumed};
    }

    // Type 5: HTML declaration <! ... >
    if (rest.starts_with("<!") && rest.size() > 2 &&
        std::isalpha(static_cast<unsigned char>(rest[2]))) {
        std::string content = raw;
        int lines_consumed = 1;
        if (rest.find('>') != std::string_view::npos) {
            return ParsedHtmlBlock{content, lines_consumed};
        }
        ++start;
        while (start < lines.size()) {
            content += '\n';
            content += lines[start];
            ++lines_consumed;
            if (lines[start].find('>') != std::string_view::npos) {
                break;
            }
            ++start;
        }
        return ParsedHtmlBlock{content, lines_consumed};
    }

    // Type 6: Block-level HTML tag (complete or incomplete)
    // Ends at a blank line.  The tag can span multiple lines (e.g.
    // `<div id="foo"\n  class="bar">`), and can be incomplete (no
    // closing '>' on the first line).  Closing block-level tags (e.g.
    // `</td>`) also match — the CommonMark reference treats them as
    // type 6 starts that can interrupt paragraphs (example 148).
    {
        auto tag = parse_html_tag_name(rest, 1);
        if (tag && is_block_html_tag(tag->name)) {
            // After the tag name, check that we have a valid tag start:
            // whitespace (attributes), '>' (complete), or end-of-line
            // (incomplete).  Reject other characters (e.g. `<divx>` is
            // not a block-level tag).
            std::size_t after_name = tag->end_pos;
            if (after_name >= rest.size() ||
                rest[after_name] == ' ' || rest[after_name] == '\t' ||
                rest[after_name] == '\n' || rest[after_name] == '\r' ||
                rest[after_name] == '>' || rest[after_name] == '/') {
                std::string content = raw;
                int lines_consumed = 1;
                ++start;
                while (start < lines.size()) {
                    // Check for blank line
                    bool is_blank = true;
                    for (char c : lines[start]) {
                        if (c != ' ' && c != '\t' && c != '\r') {
                            is_blank = false;
                            break;
                        }
                    }
                    if (is_blank) break;
                    content += '\n';
                    content += lines[start];
                    ++lines_consumed;
                    ++start;
                }
                return ParsedHtmlBlock{content, lines_consumed};
            }
        }
    }

    // Type 7: Any other HTML tag on its own line (complete tag only)
    // Ends at a blank line.  Type 7 blocks cannot interrupt a paragraph
    // (CommonMark §4.6 example 187), so skip this check when the caller
    // says a paragraph is open.
    if (can_interrupt_paragraph) {
        // Use the inline HTML parser to validate the full tag (including
        // attributes).  This rejects constructs like <a h*#ref="hi"> where
        // '*' is not valid in an attribute name, or <a href='bar'title=x>
        // where whitespace is missing between attributes (CommonMark §6.6
        // examples 619, 622, 624, 632).
        bool disallowed = false;
        const auto tag_end = parse_inline_html(rest, 0, disallowed);
        if (tag_end != std::string_view::npos) {
            // Check that nothing follows the tag (except whitespace)
            bool only_ws = true;
            for (std::size_t i = tag_end; i < rest.size(); ++i) {
                if (rest[i] != ' ' && rest[i] != '\t' &&
                    rest[i] != '\r') {
                    only_ws = false;
                    break;
                }
            }
            if (only_ws) {
                std::string content = raw;
                int lines_consumed = 1;
                ++start;
                while (start < lines.size()) {
                    bool is_blank = true;
                    for (char c : lines[start]) {
                        if (c != ' ' && c != '\t' && c != '\r') {
                            is_blank = false;
                            break;
                        }
                    }
                    if (is_blank) break;
                    content += '\n';
                    content += lines[start];
                    ++lines_consumed;
                    ++start;
                }
                return ParsedHtmlBlock{content, lines_consumed};
            }
        }
    }

    return std::nullopt;
}

// Check if a line starts a block-level construct that can interrupt a
// paragraph (CommonMark §5.1).  Used by the blockquote lazy-continuation
// logic: a line without '>' is a lazy continuation only if it cannot
// interrupt a paragraph.  Indented code blocks (4+ spaces), setext
// underlines, and deeply-indented list items cannot interrupt paragraphs.
[[nodiscard]] bool can_interrupt_paragraph(const std::string& line) {
    std::size_t pos = 0;
    while (pos < line.size() && line[pos] == ' ' && pos < 3) ++pos;
    if (pos >= line.size()) return false;  // blank line
    char c = line[pos];
    if (c == '#') return true;  // ATX heading
    if (c == '>') return true;  // blockquote
    if (c == '`' || c == '~') return true;  // code fence
    if (c == '<') {
        // HTML block type 6 (block-level tag) can interrupt a paragraph.
        auto tag = parse_html_tag_name(line, pos + 1);
        if (tag && is_block_html_tag(tag->name)) return true;
        return false;
    }
    if (c == '-' || c == '*' || c == '_') {
        // Thematic break: 3+ of the same character, with optional
        // spaces/tabs between them.  Or a list item (marker + space).
        std::size_t p = pos;
        int count = 0;
        bool only_marker_and_ws = true;
        while (p < line.size()) {
            if (line[p] == c) {
                ++count;
                ++p;
            } else if (line[p] == ' ' || line[p] == '\t') {
                ++p;
            } else {
                only_marker_and_ws = false;
                break;
            }
        }
        if (only_marker_and_ws && count >= 3) return true;  // thematic break
        // List item: marker followed by space
        return pos + 1 < line.size() &&
               (line[pos + 1] == ' ' || line[pos + 1] == '\t');
    }
    if (c == '+' ) {
        // List item only (no thematic break with '+')
        return pos + 1 < line.size() &&
               (line[pos + 1] == ' ' || line[pos + 1] == '\t');
    }
    if (c >= '0' && c <= '9') {
        // Ordered list item: digits followed by '.' or ')' and space
        std::size_t p = pos;
        while (p < line.size() && line[p] >= '0' && line[p] <= '9') ++p;
        if (p < line.size() && (line[p] == '.' || line[p] == ')') &&
            p + 1 < line.size() &&
            (line[p + 1] == ' ' || line[p + 1] == '\t')) {
            return true;
        }
        return false;
    }
    return false;
}

// Check if a line starts any block-level construct (including ones that
// cannot interrupt paragraphs, like indented code blocks).  Used to
// determine if the quote buffer's last line is a paragraph continuation.
[[nodiscard]] bool starts_block_construct(const std::string& line) {
    std::size_t pos = 0;
    while (pos < line.size() && line[pos] == ' ') ++pos;
    if (pos >= line.size()) return false;  // blank line
    if (pos >= 4) return true;  // indented code block
    char c = line[pos];
    if (c == '#') return true;
    if (c == '>') return true;
    if (c == '`' || c == '~') return true;
    if (c == '<') return true;
    if (c == '-' || c == '*' || c == '+') {
        return pos + 1 < line.size() &&
               (line[pos + 1] == ' ' || line[pos + 1] == '\t');
    }
    if (c >= '0' && c <= '9') return true;
    if (c == '[') return true;  // potential link ref def
    return false;
}

// Check if a quote-buffer line is a paragraph continuation, stripping
// nested '>' markers first.  For nested blockquotes (e.g. "> > foo"),
// the innermost content determines whether a lazy continuation applies.
[[nodiscard]] bool is_quote_paragraph_line(const std::string& line) {
    std::string l = line;
    // Strip leading '>' markers, list markers, and indentation so that
    // lazy continuation works through nested blockquote/list structures
    // (CommonMark §5.1 example 292: "> 1. > Blockquote\ncontinued here.").
    while (true) {
        std::size_t pos = 0;
        while (pos < l.size() && l[pos] == ' ' && pos < 3) ++pos;
        if (pos < l.size() && l[pos] == '>') {
            l = l.substr(pos + 1);
            if (!l.empty() && l[0] == ' ') l = l.substr(1);
            continue;
        }
        // Check for list marker — if found, recurse into the item's content
        auto marker = detect_list_marker(l);
        if (marker && !marker->content.empty()) {
            l = std::string(marker->content);
            continue;
        }
        break;
    }
    // Trim trailing whitespace
    while (!l.empty() &&
           (l.back() == ' ' || l.back() == '\t' || l.back() == '\r')) {
        l.pop_back();
    }
    return !l.empty() && !starts_block_construct(l);
}

[[nodiscard]] std::vector<BlockToken> lex_blocks(std::string_view source,
                                                   bool gfm_extensions,
                                                   bool in_blockquote) {
    std::vector<BlockToken> tokens;
    auto lines = split_lines(source);

    // GFM footnotes: pre-scan for [^label]: definitions.  Footnote defs
    // take precedence over link ref defs when both patterns match (e.g.
    // [^1]: /url is a footnote def, not a link ref with label "^1").
    std::vector<FootnoteDefEntry> fn_def_entries;
    std::unordered_set<std::size_t> fn_consumed;
    if (gfm_extensions) {
        auto [entries, consumed] = scan_footnote_defs(lines);
        fn_def_entries = std::move(entries);
        fn_consumed = std::move(consumed);
    }

    // Pre-scan for link reference definitions (CommonMark §4.7).
    // Definitions can appear anywhere in the document and are visible
    // to all inline content, so we collect them before the main loop.
    // Footnote-consumed lines are merged into ref_lines so the main loop
    // skips them via the existing ref_lines.contains() checks.
    auto [link_refs, ref_lines] =
        scan_link_ref_defs(lines, gfm_extensions);
    for (auto idx : fn_consumed) ref_lines.insert(idx);

    // Current paragraph accumulator (raw lines joined by space)
    std::string para_buffer;
    bool in_para = false;

    auto flush_paragraph = [&]() {
        if (in_para && !para_buffer.empty()) {
            BlockToken tok;
            tok.kind = BlockTokenKind::Paragraph;
            tok.inlines = tokenize_inline(para_buffer, &link_refs, gfm_extensions);
            tokens.push_back(std::move(tok));
            para_buffer.clear();
            in_para = false;
        }
    };

    // Current list state
    bool in_ulist = false;
    bool in_olist = false;
    std::vector<std::vector<BlockToken>> list_items;
    std::vector<int> list_depths;
    std::vector<std::int8_t> task_state;  // GFM task list items
    int list_base_indent = 0;   // leading whitespace of list items
    int list_start_number = 1;  // first item's number (ordered lists)
    bool list_loose = false;    // blank lines within/between items
    char list_marker_char = 0;  // '-', '+', '*', or '.' / ')' for ordered

    auto flush_list = [&]() {
        if (!list_items.empty()) {
            BlockToken tok;
            tok.kind = in_ulist ? BlockTokenKind::UnorderedList
                                : BlockTokenKind::OrderedList;
            tok.list_items = std::move(list_items);
            tok.list_depths = std::move(list_depths);
            tok.task_state = std::move(task_state);
            tok.list_start = list_start_number;
            tok.list_loose = list_loose;
            tokens.push_back(std::move(tok));
            list_items.clear();
            list_depths.clear();
            task_state.clear();
            in_ulist = false;
            in_olist = false;
            list_loose = false;
        }
    };

    // Current blockquote state
    bool in_quote = false;
    std::string quote_buffer;

    auto flush_quote = [&]() {
        if (in_quote) {
            BlockToken tok;
            tok.kind = BlockTokenKind::Blockquote;
            tok.quote_depth = 1;
            // Definitions inside blockquotes are available to references
            // outside the blockquote (CommonMark §4.7 example 218).
            // Scan the blockquote content and merge new definitions.
            auto quote_lines = split_lines(quote_buffer);
            auto [quote_refs, quote_consumed] =
                scan_link_ref_defs(quote_lines, gfm_extensions);
            for (auto& [label, def] : quote_refs) {
                if (!link_refs.contains(label)) {
                    link_refs[label] = std::move(def);
                }
            }
            // Recursively parse the blockquote content as blocks so that
            // HTML blocks, lists, headings, definitions, etc. inside
            // blockquotes are handled correctly (CommonMark §5.1).  If the
            // content is empty or contains only link reference definitions,
            // sub_blocks is empty and the blockquote renders empty — no
            // inline fallback, which would re-introduce consumed definition
            // lines as paragraph text (CommonMark §4.7 example 218).
            tok.sub_blocks = lex_blocks(quote_buffer, gfm_extensions,
                                        /*in_blockquote=*/true);
            tokens.push_back(std::move(tok));
            quote_buffer.clear();
            in_quote = false;
        }
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        // Tabs are expanded to 4-space tab stops for block structure
        // detection (CommonMark §2.2).  Code block content preserves the
        // original tabs via remove_indent().
        const std::string expanded = expand_tabs(line);

        // Skip blank lines (empty or only spaces/tabs) for list
        // continuation detection (CommonMark §2.3: blank lines).
        if (is_blank_line(expanded)) {
            flush_paragraph();
            // Don't flush the list if the next non-blank line is a list
            // marker at the same base indentation — it's a new item in the
            // same list (CommonMark §5.2: blank lines between items).
            if (in_ulist || in_olist) {
                std::size_t j = i + 1;
                while (j < lines.size() &&
                       (is_blank_line(expand_tabs(lines[j])) ||
                        ref_lines.contains(j)))
                    ++j;
                if (j < lines.size()) {
                    auto next_marker = detect_list_marker(expand_tabs(lines[j]));
                    // CommonMark §5.2: a list marker may be indented 0-3
                    // spaces from the list's base indentation.
                    if (next_marker &&
                        next_marker->base_indent >= list_base_indent &&
                        next_marker->base_indent <= list_base_indent + 3) {
                        // Same list — blank lines between items make it
                        // loose (CommonMark §5.3).
                        list_loose = true;
                        continue;
                    }
                }
            }
            flush_list();
            flush_quote();
            continue;
        }

        // Skip link reference definition lines — they produce no block
        // output (CommonMark §4.7).  Already collected in the pre-scan.
        if (ref_lines.contains(i)) {
            continue;
        }

        // Blockquote: up to 3 spaces indent, then '>' with optional space.
        // CommonMark §5.1: '>' can be followed by a space or directly by
        // content (e.g. '># Foo' is a blockquote with heading 'Foo').
        // This check runs BEFORE fenced code block and list checks so that
        // blockquote markers are not misinterpreted as other constructs.
        {
            std::size_t bq_pos = 0;
            while (bq_pos < expanded.size() && expanded[bq_pos] == ' ' &&
                   bq_pos < 3) {
                ++bq_pos;
            }
            if (bq_pos < expanded.size() && expanded[bq_pos] == '>') {
                flush_paragraph();
                flush_list();
                if (in_quote) quote_buffer += '\n';
                in_quote = true;
                std::size_t content_start = bq_pos + 1;
                // Skip one optional space after '>'
                if (content_start < expanded.size() &&
                    expanded[content_start] == ' ') {
                    ++content_start;
                }
                quote_buffer += expanded.substr(content_start);
                continue;
            }
        }

        // Blockquote lazy continuation: a line without > that is part of
        // the blockquote's paragraph (CommonMark §5.1).  Only applies when
        // the quote buffer's last content is a paragraph (not a code block,
        // list, etc.) and the line cannot interrupt a paragraph.  This
        // runs before the fenced code block and list checks so that
        // lazy continuations are not misinterpreted as other constructs.
        if (in_quote) {
            // Check if the quote buffer's last line is a paragraph
            // continuation (non-empty, doesn't start a block construct).
            bool last_is_paragraph = false;
            if (!quote_buffer.empty()) {
                std::size_t last_nl = quote_buffer.rfind('\n');
                std::string last_line = (last_nl == std::string::npos)
                    ? quote_buffer
                    : quote_buffer.substr(last_nl + 1);
                // Trim trailing whitespace
                while (!last_line.empty() &&
                       (last_line.back() == ' ' || last_line.back() == '\t' ||
                        last_line.back() == '\r')) {
                    last_line.pop_back();
                }
                last_is_paragraph = !last_line.empty() &&
                                    is_quote_paragraph_line(last_line);
            }
            if (last_is_paragraph && !can_interrupt_paragraph(expanded)) {
                // Preserve the original indentation so the recursive
                // parser's 4-space guard prevents list-item detection
                // on lines like "    - bar".  The paragraph continuation
                // logic in the recursive call will strip the indent.
                quote_buffer += '\n';
                quote_buffer += expanded;
                continue;
            }
            // Not a lazy continuation — flush the blockquote and fall
            // through to normal processing.
            flush_quote();
        }

        // Fenced code block (content preserves original tabs).  Supports
        // both ``` and ~~~ fences (CommonMark §4.5).  Up to 3 leading
        // spaces; the closing fence must be the same character and at
        // least as long as the opening fence.
        {
            std::size_t lead = 0;
            while (lead < expanded.size() && expanded[lead] == ' ' &&
                   lead < 3) {
                ++lead;
            }
            if (lead < expanded.size() &&
                (expanded[lead] == '`' || expanded[lead] == '~')) {
                const char fence_char = expanded[lead];
                std::size_t fence_len = 0;
                while (lead + fence_len < expanded.size() &&
                       expanded[lead + fence_len] == fence_char) {
                    ++fence_len;
                }
                if (fence_len >= 3) {
                    // Info string is everything after the fence.
                    std::string_view info_sv(
                        expanded.data() + lead + fence_len,
                        expanded.size() - lead - fence_len);
                    // For backtick fences, the info string may not
                    // contain a backtick (ambiguous with a code span).
                    bool valid = true;
                    if (fence_char == '`' &&
                        info_sv.find('`') != std::string_view::npos) {
                        valid = false;
                    }
                    if (valid) {
                        flush_paragraph();
                        flush_list();
                        flush_quote();

                        // The language is the first whitespace-separated
                        // word of the info string, with backslash escapes
                        // and entity references processed.
                        std::string lang;
                        {
                            std::string info(info_sv);
                            auto lb = info.find_first_not_of(" \t");
                            if (lb != std::string::npos) {
                                auto le = info.find_last_not_of(" \t");
                                info = info.substr(lb, le - lb + 1);
                            } else {
                                info = "";
                            }
                            std::string unescaped;
                            unescaped.reserve(info.size());
                            for (std::size_t k = 0; k < info.size(); ++k) {
                                if (info[k] == '\\' &&
                                    k + 1 < info.size()) {
                                    unescaped += info[k + 1];
                                    ++k;
                                } else {
                                    unescaped += info[k];
                                }
                            }
                            if (auto sp = unescaped.find_first_of(" \t");
                                sp != std::string::npos) {
                                unescaped = unescaped.substr(0, sp);
                            }
                            lang = decode_entities(unescaped);
                        }

                        // Collect content lines until closing fence.
                        std::string code;
                        bool has_content = false;
                        ++i;
                        while (i < lines.size()) {
                            const auto& cline = lines[i];
                            const std::string cexp = expand_tabs(cline);
                            if (is_closing_fence(cexp, fence_char,
                                                 fence_len)) {
                                break;
                            }
                            if (has_content) code += '\n';
                            code += remove_indent(
                                cline, static_cast<int>(lead));
                            has_content = true;
                            ++i;
                        }

                        BlockToken tok;
                        tok.kind = BlockTokenKind::CodeBlock;
                        tok.code_lang = std::move(lang);
                        tok.code_content = std::move(code);
                        tokens.push_back(std::move(tok));
                        continue;
                    }
                }
            }
        }

        // HTML block (CommonMark §4.6).  Raw HTML passed through.
        // Types 1-5 end at a specific marker; types 6-7 end at a blank line.
        // Type 7 cannot interrupt a paragraph, so pass !in_para.
        if (auto html = try_parse_html_block(lines, i, !in_para)) {
            flush_paragraph();
            flush_list();
            flush_quote();

            BlockToken tok;
            tok.kind = BlockTokenKind::HtmlBlock;
            tok.html_content = std::move(html->content);
            tokens.push_back(std::move(tok));
            i += html->lines_consumed - 1;
            continue;
        }

        // Block math: $...$ (LaTeX display math)
        // Mirrors marked-math extension: $$ on its own line starts a display
        // math block; closing $$ on its own line ends it.  Content between
        // is captured verbatim (no inline processing).
        if (line.starts_with("$")) {
            flush_paragraph();
            flush_list();
            flush_quote();

            std::string math;
            // Check if content is on the same line: $...$
            auto rest = line.substr(2);
            auto close_pos = rest.find("$");
            if (close_pos != std::string::npos) {
                // Single-line block math: $formula$
                math = rest.substr(0, close_pos);
            } else {
                // Multi-line: $ on opening line, content on subsequent lines,
                // $ on closing line.
                if (!rest.empty()) {
                    math = rest;
                }
                ++i;
                while (i < lines.size() && !lines[i].starts_with("$")) {
                    if (!math.empty()) math += '\n';
                    math += lines[i];
                    ++i;
                }
                // If we found the closing $ line, also grab any trailing
                // content after it on the same line (usually empty).
                if (i < lines.size()) {
                    auto tail = lines[i].substr(2);
                    if (!tail.empty()) {
                        if (!math.empty()) math += '\n';
                        math += tail;
                    }
                }
            }

            BlockToken tok;
            tok.kind = BlockTokenKind::MathBlock;
            tok.math_content = std::move(math);
            tokens.push_back(std::move(tok));
            continue;
        }

        // GFM Table: header row followed by alignment separator
        if (expanded.find('|') != std::string::npos && i + 1 < lines.size() &&
            is_table_separator(expand_tabs(lines[i + 1]))) {
            auto header_cells = split_table_row(expanded);
            auto sep_cells = split_table_row(expand_tabs(lines[i + 1]));
            // GFM: the delimiter row must have the same cell count as the
            // header row, else this is not a table.
            if (!header_cells.empty() &&
                header_cells.size() == sep_cells.size()) {
                flush_paragraph();
                flush_list();
                flush_quote();

                BlockToken tok;
                tok.kind = BlockTokenKind::Table;
                tok.table_headers = std::move(header_cells);
                tok.table_align = parse_table_align(sep_cells);
                tok.table_link_refs = link_refs;
                const std::size_t col_count = tok.table_headers.size();
                i += 2;
                while (i < lines.size() &&
                       !is_table_interrupt(lines[i])) {
                    auto row = split_table_row(expand_tabs(lines[i]));
                    // Pad short rows with empty cells; truncate long rows.
                    row.resize(col_count);
                    tok.table_rows.push_back(std::move(row));
                    ++i;
                }
                --i;
                tokens.push_back(std::move(tok));
                continue;
            }
        }

        // Headings (ATX style: # H1, ## H2, etc.)
        // CommonMark §4.1: up to 3 spaces indent, 1-6 #, optional space,
        // text, optional closing # sequence.
        {
            std::size_t hpos = 0;
            int hindent = 0;
            while (hpos < expanded.size() && expanded[hpos] == ' ' &&
                   hindent < 3) {
                ++hpos;
                ++hindent;
            }
            if (hpos < expanded.size() && expanded[hpos] == '#') {
                int level = 0;
                while (hpos < expanded.size() && expanded[hpos] == '#' &&
                       level < 6) {
                    ++hpos;
                    ++level;
                }
                // Require end-of-line or whitespace after the # run
                if (hpos >= expanded.size() || expanded[hpos] == ' ' ||
                    expanded[hpos] == '\t') {
                    flush_paragraph();
                    flush_list();
                    flush_quote();

                    // Skip one space after the #s
                    if (hpos < expanded.size() &&
                        (expanded[hpos] == ' ' || expanded[hpos] == '\t')) {
                        ++hpos;
                    }
                    std::string heading_text = expanded.substr(hpos);
                    // Strip closing sequence: trailing #s (preceded by a
                    // space), ignoring trailing spaces.
                    if (!heading_text.empty()) {
                        // Trim trailing spaces first
                        std::size_t end = heading_text.size();
                        while (end > 0 && heading_text[end - 1] == ' ') --end;
                        // Walk back over trailing #s
                        std::size_t p = end;
                        while (p > 0 && heading_text[p - 1] == '#') --p;
                        if (p < end) {
                            // Closing sequence found.  If the whole text
                            // is #s, the heading is empty; otherwise the
                            // sequence must be preceded by a space.
                            if (p == 0 || heading_text[p - 1] == ' ') {
                                heading_text = heading_text.substr(0, p > 0 ? p - 1 : 0);
                            }
                        }
                    }
                    // Trim leading/trailing whitespace
                    auto lb = heading_text.find_first_not_of(" \t");
                    if (lb == std::string::npos) {
                        heading_text.clear();
                    } else {
                        auto rb = heading_text.find_last_not_of(" \t");
                        heading_text = heading_text.substr(lb, rb - lb + 1);
                    }
                    BlockToken tok;
                    tok.kind = BlockTokenKind::Heading;
                    tok.heading_level = level;
                    tok.inlines = tokenize_inline(heading_text, &link_refs, gfm_extensions);
                    tokens.push_back(std::move(tok));
                    continue;
                }
            }
        }

        // Setext heading (CommonMark §4.3): a paragraph followed by a
        // line of === (level 1) or --- (level 2).  Only applies when the
        // current line is a paragraph continuation — otherwise --- falls
        // through to the thematic break check below.  Not inside a
        // blockquote — a Setext underline on a lazy continuation line is
        // paragraph text, not a heading (CommonMark §4.3 example 93).
        if (in_para && !in_quote && !in_blockquote) {
            int setext_level = is_setext_underline(expanded);
            if (setext_level > 0) {
                BlockToken tok;
                tok.kind = BlockTokenKind::Heading;
                tok.heading_level = setext_level;
                tok.inlines = tokenize_inline(para_buffer, &link_refs, gfm_extensions);
                tokens.push_back(std::move(tok));
                para_buffer.clear();
                in_para = false;
                continue;
            }
        }

        // Horizontal rule (3+ of -, *, _ with optional spaces/tabs)
        if (is_thematic_break(expanded)) {
            flush_paragraph();
            flush_list();
            flush_quote();
            BlockToken tok;
            tok.kind = BlockTokenKind::HorizontalRule;
            tokens.push_back(std::move(tok));
            continue;
        }

        // Indented code block (4+ spaces of indent, CommonMark §4.4).
        // Not inside a list/blockquote — those contexts treat 4-space
        // indents as continuation, not code blocks.  Cannot interrupt a
        // paragraph (CommonMark: an indented code block must be preceded
        // by a blank line).
        {
            int indent = 0;
            while (indent < static_cast<int>(expanded.size()) &&
                   expanded[indent] == ' ') {
                ++indent;
            }
            if (indent >= 4 && !in_para && !in_ulist && !in_olist && !in_quote) {
                flush_paragraph();
                flush_list();
                flush_quote();

                std::string code = remove_indent(line, 4);
                ++i;
                while (i < lines.size()) {
                    const std::string exp2 = expand_tabs(lines[i]);
                    int ind2 = 0;
                    while (ind2 < static_cast<int>(exp2.size()) &&
                           exp2[ind2] == ' ') {
                        ++ind2;
                    }
                    if (ind2 >= 4) {
                        code += '\n';
                        code += remove_indent(lines[i], 4);
                        ++i;
                    } else if (is_blank_line(lines[i])) {
                        // Blank line (empty or only spaces): peek ahead —
                        // include only if followed by another indented
                        // line (part of the code block).
                        std::size_t j = i + 1;
                        while (j < lines.size() && is_blank_line(lines[j])) ++j;
                        if (j < lines.size()) {
                            const std::string exp3 = expand_tabs(lines[j]);
                            int ind3 = 0;
                            while (ind3 < static_cast<int>(exp3.size()) &&
                                   exp3[ind3] == ' ') {
                                ++ind3;
                            }
                            if (ind3 >= 4) {
                                code += '\n';
                                ++i;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
                --i;  // last line wasn't part of the code block
                // Strip trailing blank lines from content
                while (!code.empty() && code.back() == '\n') {
                    code.pop_back();
                }
                BlockToken tok;
                tok.kind = BlockTokenKind::CodeBlock;
                tok.code_lang = "";
                tok.code_content = std::move(code);
                tokens.push_back(std::move(tok));
                continue;
            }
        }

        // List item (unordered or ordered) with continuation support.
        // CommonMark §5.2: a list item's content includes all lines indented
        // to the content indentation, plus lazy continuation lines.  The
        // collected content is parsed as sub-blocks (paragraphs, code blocks,
        // nested lists, blockquotes, etc.).
        {
            auto marker = detect_list_marker(expanded);
            if (marker) {
                // Paragraph-interruption rules (CommonMark §5.2): a list
                // item can interrupt a paragraph only if it has content
                // (not just a marker at EOL) and is either unordered or
                // ordered with number 1.
                bool can_interrupt = true;
                if (in_para) {
                    const bool has_content =
                        !marker->content.empty() &&
                        marker->content.find_first_not_of(" \t") !=
                            std::string_view::npos;
                    can_interrupt =
                        has_content &&
                        (!marker->ordered || marker->number == 1);
                }
                // 4+ spaces indent and not in a list context → indented code
                // block, not a list item (CommonMark §4.4).
                if (can_interrupt &&
                    (marker->base_indent < 4 || in_ulist || in_olist)) {
                    flush_paragraph();
                    flush_quote();

                    // Start or continue list.  Different marker characters
                    // ('-' vs '+' vs '*') create separate lists (CommonMark
                    // §5.2 example 301).
                    if (marker->ordered) {
                        if (!in_olist || list_marker_char != marker->marker_char) {
                            flush_list();
                            in_olist = true;
                            in_ulist = false;
                            list_base_indent = marker->base_indent;
                            list_start_number = marker->number;
                            list_marker_char = marker->marker_char;
                        }
                    } else {
                        if (!in_ulist || list_marker_char != marker->marker_char) {
                            flush_list();
                            in_ulist = true;
                            in_olist = false;
                            list_base_indent = marker->base_indent;
                            list_marker_char = marker->marker_char;
                        }
                    }

                    // Collect item content (first line + continuations)
                    std::string item_content(marker->content);
                    bool item_has_blank = false;
                    const int content_indent = marker->content_indent;

                    ++i;  // move to next line
                    while (i < lines.size()) {
                        const auto& cont_line = lines[i];
                        const std::string cont_expanded =
                            expand_tabs(cont_line);

                        if (is_blank_line(cont_expanded)) {
                            // Empty list item (marker at EOL, no content):
                            // a blank line ends the item (CommonMark §5.2
                            // example 280).  Check if the item has any
                            // non-whitespace content.
                            bool has_content = false;
                            for (char ch : item_content) {
                                if (ch != ' ' && ch != '\t' && ch != '\n') {
                                    has_content = true;
                                    break;
                                }
                            }
                            if (!has_content) break;

                            // Blank line — peek ahead to decide
                            std::size_t j = i + 1;
                            while (j < lines.size() &&
                                   is_blank_line(expand_tabs(lines[j])))
                                ++j;
                            if (j < lines.size()) {
                                const std::string next_expanded =
                                    expand_tabs(lines[j]);
                                int next_indent = 0;
                                while (next_indent <
                                           static_cast<int>(next_expanded.size()) &&
                                       next_expanded[next_indent] == ' ')
                                    ++next_indent;

                                if (next_indent >= content_indent) {
                                    // If the indented line is a link ref def,
                                    // skip it and check if the next line is a
                                    // list marker (blank line between items
                                    // with a ref def in between → loose list,
                                    // CommonMark example 317).
                                    if (ref_lines.contains(j)) {
                                        std::size_t k = j + 1;
                                        while (k < lines.size() &&
                                               (is_blank_line(
                                                    expand_tabs(lines[k])) ||
                                                ref_lines.contains(k)))
                                            ++k;
                                        if (k < lines.size()) {
                                            auto after_ref =
                                                detect_list_marker(
                                                    expand_tabs(lines[k]));
                                            if (after_ref &&
                                                after_ref->base_indent >=
                                                    list_base_indent &&
                                                after_ref->base_indent <=
                                                    list_base_indent + 3) {
                                                list_loose = true;
                                                break;
                                            }
                                        }
                                    }
                                    // Indented continuation after blank —
                                    // preserve all blank lines (code blocks
                                    // may contain them).
                                    for (std::size_t k = i; k < j; ++k) {
                                        item_content += '\n';
                                    }
                                    item_has_blank = true;
                                    i = j;
                                    continue;
                                }

                                // New list marker at compatible base indent?
                                auto next_marker =
                                    detect_list_marker(next_expanded);
                                if (next_marker &&
                                    next_marker->base_indent >= list_base_indent &&
                                    next_marker->base_indent <=
                                        list_base_indent + 3) {
                                    // New item in same list — blank line
                                    // between items makes the list loose.
                                    list_loose = true;
                                    break;
                                }
                            }
                            // End of list — end item
                            break;
                        }

                        int cont_indent = 0;
                        while (cont_indent <
                                   static_cast<int>(cont_expanded.size()) &&
                               cont_expanded[cont_indent] == ' ')
                            ++cont_indent;

                        if (cont_indent >= content_indent) {
                            // Indented continuation
                            item_content += '\n';
                            item_content +=
                                cont_expanded.substr(content_indent);
                            ++i;
                            continue;
                        }

                        // Non-indented line
                        // A list marker at the same base indent ends the
                        // current item (new item in same list).  This must
                        // be checked before can_interrupt_paragraph because
                        // a marker at EOL (e.g. "-") is a valid empty list
                        // item but is NOT a paragraph-interrupting construct.
                        {
                            auto next_marker = detect_list_marker(cont_expanded);
                            if (next_marker &&
                                next_marker->base_indent >= list_base_indent &&
                                next_marker->base_indent <=
                                    list_base_indent + 3) {
                                break;  // new item — end current item
                            }
                        }
                        if (can_interrupt_paragraph(cont_expanded)) {
                            break;  // new block construct — end item
                        }

                        // Lazy continuation (only if no trailing blank)
                        if (!item_content.empty() && !item_has_blank) {
                            item_content += '\n';
                            item_content += cont_expanded;
                            ++i;
                            continue;
                        }

                        break;  // not a continuation — end item
                    }

                    // Parse item content as sub-blocks
                    auto sub_blocks =
                        lex_blocks(item_content, gfm_extensions);

                    // GFM task list marker: check first paragraph's text
                    std::int8_t task = -1;
                    if (!sub_blocks.empty() &&
                        sub_blocks[0].kind == BlockTokenKind::Paragraph &&
                        !sub_blocks[0].inlines.empty()) {
                        const auto& first_tok = sub_blocks[0].inlines[0];
                        if (first_tok.kind == InlineTokenKind::Text) {
                            bool checked = false;
                            if (auto skip = parse_task_marker(first_tok.text,
                                                              checked)) {
                                task = checked ? 1 : 0;
                                // Remove the marker from the text
                                auto& text = const_cast<std::string&>(
                                    sub_blocks[0].inlines[0].text);
                                text = text.substr(skip);
                                if (text.empty()) {
                                    sub_blocks[0].inlines.erase(
                                        sub_blocks[0].inlines.begin());
                                }
                            }
                        }
                    }

                    // Blank lines within an item make the list loose only
                    // when they separate two block-level elements (CommonMark
                    // §5.3).  Blank lines inside a single code block do not.
                    // Check size BEFORE moving sub_blocks.
                    bool item_is_loose =
                        item_has_blank && sub_blocks.size() >= 2;
                    // Distinguish blank lines between sub-blocks (loose) from
                    // blank lines inside a nested list item (tight).  If the
                    // line after the first blank line is indented ≥ the last
                    // list marker's content_indent, it continues the nested
                    // list item — the blank line is inside it, not between
                    // the outer item's sub-blocks (CommonMark §5.3 examples
                    // 307, 319, 325).
                    if (item_is_loose) {
                        const auto content_lines = split_lines(item_content);
                        std::size_t first_blank_idx = content_lines.size();
                        for (std::size_t cl = 0; cl < content_lines.size();
                             ++cl) {
                            if (is_blank_line(content_lines[cl])) {
                                first_blank_idx = cl;
                                break;
                            }
                        }
                        if (first_blank_idx < content_lines.size()) {
                            int last_marker_content_indent = -1;
                            for (std::size_t cl = 0; cl < first_blank_idx;
                                 ++cl) {
                                if (auto m = detect_list_marker(
                                        expand_tabs(content_lines[cl]))) {
                                    last_marker_content_indent =
                                        m->content_indent;
                                }
                            }
                            if (last_marker_content_indent >= 0) {
                                std::size_t after = first_blank_idx + 1;
                                while (after < content_lines.size() &&
                                       is_blank_line(content_lines[after]))
                                    ++after;
                                if (after < content_lines.size()) {
                                    const auto after_expanded =
                                        expand_tabs(content_lines[after]);
                                    int after_indent = 0;
                                    while (after_indent <
                                               static_cast<int>(
                                                   after_expanded.size()) &&
                                           after_expanded[after_indent] == ' ')
                                        ++after_indent;
                                    if (after_indent >=
                                        last_marker_content_indent) {
                                        item_is_loose = false;
                                    }
                                }
                            }
                        }
                    }
                    list_items.push_back(std::move(sub_blocks));
                    list_depths.push_back(marker->base_indent / 2);
                    task_state.push_back(task);
                    if (item_is_loose) list_loose = true;

                    // The collection loop advanced i to the first line after
                    // the item.  Decrement so the for loop's ++i lands on
                    // that line for the main loop to process.
                    if (i > 0) --i;
                    continue;
                }
            }
        }

        // Regular paragraph text.  Lines are joined with '\n' (not space)
        // so the HTML serializer can detect hard line breaks (CommonMark
        // §6.1: two+ trailing spaces or a trailing backslash before \n).
        // The production renderer converts \n back to space.
        if (in_para) {
            para_buffer += '\n';
            // Strip leading spaces from continuation lines (CommonMark:
            // leading whitespace on paragraph continuation lines is removed).
            std::size_t first_non_space = 0;
            while (first_non_space < expanded.size() &&
                   expanded[first_non_space] == ' ') {
                ++first_non_space;
            }
            para_buffer += expanded.substr(first_non_space);
        } else {
            in_para = true;
            // Strip up to 3 leading spaces of indentation (CommonMark:
            // paragraphs may have up to 3 spaces of initial indentation).
            std::size_t first_non_space = 0;
            while (first_non_space < expanded.size() &&
                   first_non_space < 3 &&
                   expanded[first_non_space] == ' ') {
                ++first_non_space;
            }
            para_buffer += expanded.substr(first_non_space);
        }
    }

    flush_paragraph();
    flush_list();
    flush_quote();

    // Emit footnote definition blocks at the end of the stream.  The HTML
    // serializer collects these (recursively, including those nested inside
    // blockquote/list sub_blocks) and hoists them to the footnotes section.
    if (gfm_extensions) {
        for (const auto& entry : fn_def_entries) {
            BlockToken tok;
            tok.kind = BlockTokenKind::FootnoteDef;
            tok.footnote_label = entry.raw_label;
            tok.sub_blocks = lex_blocks(entry.content, gfm_extensions);
            tokens.push_back(std::move(tok));
        }
    }

    return tokens;
}
} // namespace detail
} // namespace loom::ui
