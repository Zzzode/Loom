module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// An event-modeling diagram is an ordered list of time frames
// (`tf NN kind name` with optional inline data, a data-block reference or
// source relations) plus named `data` blocks. The terminal has no
// event-stream geometry or color contract, so the projection is a bounded
// native outline: each statement is shape-validated and the physical lines
// are transcribed verbatim, preserving the column layout. No geometry, no
// color.

constexpr int kEmMaxStatements = 512;
constexpr int kEmMaxRows = 1024;
constexpr int kEmMaxTextBytes = 512;
constexpr int kEmBlockIndent = 2;

bool em_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view em_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    std::size_t search = 4;
    while (true) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                                : nl - search);
        std::size_t i = 0;
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i == line.size()) {
                if (nl == std::string_view::npos) return std::string_view{};
                return source.substr(nl + 1);
            }
        }
        if (nl == std::string_view::npos) return source;
        search = nl + 1;
    }
}

std::string_view trim_spaces(std::string_view value) {
    while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    while (!value.empty() && value.back() == ' ') value.remove_suffix(1);
    return value;
}

bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}
bool is_digit(char c) { return c >= '0' && c <= '9'; }

// An identifier, optionally dotted with namespace segments
// (`Inventory.AddItem`). Every segment must start with an identifier-start
// character; no leading/trailing/double dot and no digit-first segment.
bool is_dotted_name(std::string_view token) {
    if (token.empty() || token.size() > static_cast<std::size_t>(kEmMaxTextBytes))
        return false;
    bool at_segment_start = true;
    bool segment_has_char = false;
    for (char c : token) {
        if (c == '.') {
            if (at_segment_start || !segment_has_char) return false;
            at_segment_start = true;
            segment_has_char = false;
        } else {
            if (at_segment_start && !is_ident_start(c)) return false;
            if (!is_ident_char(c)) return false;
            at_segment_start = false;
            segment_has_char = true;
        }
    }
    return !at_segment_start && segment_has_char;
}

// A frame index in the corpus is exactly two zero-padded digits.
bool is_frame_index(std::string_view token) {
    return token.size() == 2 && is_digit(token[0]) && is_digit(token[1]);
}

// Printable, single-line ASCII text. Control bytes, DEL and non-ASCII are
// rejected rather than emitted as invisible cells.
bool is_printable(std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(kEmMaxTextBytes))
        return false;
    return std::ranges::all_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u >= 0x20 && u <= 0x7E;
    });
}

// Parse the part of a frame line after `tf NN kind `: a dotted name followed
// by zero or more suffixes (inline `{...}`, a `[[...]]` reference, or one or
// more `->> NN` relations).
bool parse_frame_tail(std::string_view tail) {
    std::string_view rest = trim_spaces(tail);

    // Mandatory dotted name up to the first suffix marker or space.
    std::size_t end = 0;
    while (end < rest.size() && rest[end] != ' ' && rest[end] != '{' &&
           rest[end] != '[' && rest[end] != '-')
        ++end;
    if (!is_dotted_name(rest.substr(0, end))) return false;
    rest = trim_spaces(rest.substr(end));

    bool saw_inline = false;
    bool saw_ref = false;
    while (!rest.empty()) {
        if (rest.starts_with('{')) {
            // Single-line inline data; no nested brace and a printable body.
            if (saw_inline) return false;  // at most one inline block
            const auto close = rest.find('}');
            if (close == std::string_view::npos || rest.find('{', 1) != std::string_view::npos)
                return false;
            if (!is_printable(trim_spaces(rest.substr(1, close - 1)))) return false;
            saw_inline = true;
            rest = trim_spaces(rest.substr(close + 1));
        } else if (rest.starts_with("[[")) {
            if (saw_ref) return false;  // at most one data-block reference
            const auto close = rest.find("]]");
            if (close == std::string_view::npos) return false;
            if (!is_dotted_name(trim_spaces(rest.substr(2, close - 2)))) return false;
            saw_ref = true;
            rest = trim_spaces(rest.substr(close + 2));
        } else if (rest.starts_with("->>")) {
            rest = trim_spaces(rest.substr(3));
            const auto sp = rest.find(' ');
            const std::string_view index =
                sp == std::string_view::npos ? rest : rest.substr(0, sp);
            if (!is_frame_index(index)) return false;
            rest = sp == std::string_view::npos ? std::string_view{}
                                                 : trim_spaces(rest.substr(sp + 1));
        } else {
            return false;  // unknown suffix / leftover token
        }
    }
    return true;
}

bool is_frame(std::string_view content) {
    if (!content.starts_with("tf ")) return false;
    std::string_view rest = trim_spaces(content.substr(3));
    const auto sp1 = rest.find(' ');
    if (sp1 == std::string_view::npos || !is_frame_index(rest.substr(0, sp1)))
        return false;
    rest = trim_spaces(rest.substr(sp1 + 1));
    const auto sp2 = rest.find(' ');
    if (sp2 == std::string_view::npos) return false;
    const std::string_view kind = rest.substr(0, sp2);
    if (kind != "ui" && kind != "cmd" && kind != "evt" && kind != "rmo")
        return false;
    return parse_frame_tail(rest.substr(sp2 + 1));
}

// A data-block content line: `key: value` with at most one optional
// trailing comma. A doubled trailing comma (`k: v,,`) fails closed.
bool is_data_entry(std::string_view content) {
    const auto colon = content.find(':');
    if (colon == std::string_view::npos) return false;
    if (!is_dotted_name(trim_spaces(content.substr(0, colon)))) return false;
    std::string_view value = trim_spaces(content.substr(colon + 1));
    if (value.ends_with(',')) value = trim_spaces(value.substr(0, value.size() - 1));
    if (value.empty() || value.ends_with(',')) return false;
    return is_printable(value);
}

}  // namespace

std::optional<std::vector<std::string>> eventmodeling_layout(std::string_view source,
                                                             Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!em_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = em_body(source);
    // No event-stream/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. Trim the key before comparing so the YAML-legal
    // "config : 42" spacing shape still gates.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            std::size_t search = 4;
            while (search < source.size()) {
                const auto nl = source.find('\n', search);
                const std::string_view line =
                    source.substr(search, nl == std::string_view::npos
                                                      ? std::string_view::npos
                                                      : nl - search);
                std::size_t i = 0;
                bool is_fence = false;
                if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
                    i = 3;
                    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
                    is_fence = i == line.size();
                }
                const auto key_start = line.find_first_not_of(' ');
                if (!is_fence && key_start != std::string_view::npos) {
                    std::string_view key = line.substr(key_start);
                    const auto colon = key.find(':');
                    if (colon != std::string_view::npos)
                        key = trim_spaces(key.substr(0, colon));
                    if (key == "theme" || key == "look" || key == "config")
                        visual_gate = true;
                }
                if (is_fence) break;
                if (nl == std::string_view::npos) break;
                search = nl + 1;
            }
        }
        if (!visual_gate) {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
                pos = eol == std::string_view::npos ? body.size() : eol + 1;
                const auto trimmed = line.find_first_not_of(' ');
                if (trimmed != std::string_view::npos) {
                    const std::string_view content = line.substr(trimmed);
                    if (content.starts_with("%%{init") ||
                        content.starts_with("classDef") ||
                        content.starts_with("style ") ||
                        content.find(":::") != std::string_view::npos)
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    bool block_open = false;
    bool block_has_entry = false;
    bool expect_brace = false;
    int statement_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kEmMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(row);
        return true;
    };

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                            : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto first_non_space = line.find_first_not_of(' ');
        if (first_non_space == std::string_view::npos) continue;  // blank line
        const std::string_view content = line.substr(first_non_space);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (first_non_space != 0 || content != "eventmodeling") return {};
            saw_header = true;
            continue;
        }

        bool accepted = false;
        if (block_open) {
            // Inside a data block: only indent-2 entries or a column-0 close.
            if (first_non_space == 0 && content == "}") {
                if (!block_has_entry) return {};  // an empty block is rejected
                block_open = false;
                block_has_entry = false;
                accepted = true;
            } else if (first_non_space == kEmBlockIndent) {
                accepted = is_data_entry(content);
                if (accepted) block_has_entry = true;
            }
        } else if (first_non_space == 0) {
            if (content == "{") {
                if (!expect_brace) return {};
                expect_brace = false;
                block_open = true;
                block_has_entry = false;
                accepted = true;
            } else if (content.starts_with("data ")) {
                // A data header must be followed by its own block; a second
                // header or frame cannot overwrite the pending expectation.
                if (expect_brace) return {};
                accepted = is_dotted_name(trim_spaces(content.substr(5)));
                expect_brace = true;
            } else if (content.starts_with("tf ")) {
                if (expect_brace) return {};
                accepted = is_frame(content);
            }
        }
        if (!accepted) return {};
        if (++statement_count > kEmMaxStatements) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header || block_open || expect_brace) return {};  // unclosed block
    if (out.empty()) {
        if (!push_guarded("eventmodeling")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("eventmodeling");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
