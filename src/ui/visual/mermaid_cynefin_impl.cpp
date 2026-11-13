module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A cynefin-beta diagram is a strict two-space indented frame: the five
// domain keywords (complex, complicated, clear, chaotic, confusion), quoted
// items, title/accessibility directives and labeled domain transitions. The
// terminal has no boundary-curve geometry or color contract, so the
// projection is a bounded native outline that validates each statement and
// transcribes the line verbatim, preserving its source indentation. No
// geometry, no color.

constexpr int kCynefinMaxStatements = 512;
constexpr int kCynefinMaxTextBytes = 512;
constexpr int kCynefinMaxRows = 1024;
constexpr int kCynefinIndent = 2;

bool cynefin_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view cynefin_body(std::string_view source) {
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

bool is_domain_keyword(std::string_view statement) {
    return statement == "complex" || statement == "complicated" ||
           statement == "clear" || statement == "chaotic" ||
           statement == "confusion";
}

// Frame text is the ASCII character set observed in the corpus for titles,
// accessibility directives, quoted items and transition labels: letters,
// digits, spaces and the punctuation ( ) - /. A quote, structural marker,
// non-ASCII byte or emoji means the source must fall back.
bool is_frame_text(std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(kCynefinMaxTextBytes))
        return false;
    if (!unicode_text(std::string{text})) return false;
    for (char c : text) {
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '(' || c == ')' ||
            c == '-' || c == '/';
        if (!allowed) return false;
    }
    return true;
}

// A quoted item or transition label: exactly "text" with a non-empty frame
// text interior and no embedded quote.
bool parse_quoted(std::string_view statement, std::string_view& interior) {
    if (statement.size() < 2 || statement.front() != '"' ||
        statement.back() != '"')
        return false;
    interior = statement.substr(1, statement.size() - 2);
    if (interior.empty() || interior.find('"') != std::string_view::npos)
        return false;
    return is_frame_text(interior);
}

// A transition: <domain> --> <domain> : "<label>". Self-loops use the same
// shape and are transcribed verbatim (the upstream renderer drops their
// arrow, but the terminal outline has no arrow geometry to begin with).
bool parse_transition(std::string_view statement) {
    const auto arrow = statement.find("-->");
    if (arrow == std::string_view::npos) return false;
    const auto source_end = trim_spaces(statement.substr(0, arrow));
    const auto after_arrow = statement.substr(arrow + 3);
    const auto colon = after_arrow.find(':');
    if (colon == std::string_view::npos) return false;
    const auto target = trim_spaces(after_arrow.substr(0, colon));
    const auto label = trim_spaces(after_arrow.substr(colon + 1));
    if (!is_domain_keyword(source_end) || !is_domain_keyword(target))
        return false;
    std::string_view interior;
    return parse_quoted(label, interior);
}

}  // namespace

std::optional<std::vector<std::string>> cynefin_layout(std::string_view source,
                                                       Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!cynefin_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = cynefin_body(source);
    // No boundary/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. The fence splitter tolerates trailing whitespace.
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
                    // YAML permits whitespace before the mapping colon
                    // ("config : 42"); trim so the visual key still gates.
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
    int statement_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kCynefinMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
            if (content != "cynefin-beta") return {};
            saw_header = true;
            continue;
        }
        // Every statement sits on the strict two-space grid (tabs were
        // pre-rejected); indent 0/1 or anything deeper than two fails closed.
        if (first_non_space != static_cast<std::size_t>(kCynefinIndent)) return {};
        if (++statement_count > kCynefinMaxStatements) return {};

        std::string_view interior;
        bool accepted = false;
        if (is_domain_keyword(content)) {
            accepted = true;
        } else if (content.starts_with('"')) {
            accepted = parse_quoted(content, interior);
        } else if (content.starts_with("title ")) {
            accepted = is_frame_text(content.substr(6));
        } else if (content.starts_with("accTitle: ")) {
            accepted = is_frame_text(content.substr(10));
        } else if (content.starts_with("accDescr: ")) {
            accepted = is_frame_text(content.substr(10));
        } else if (content.find("-->") != std::string_view::npos) {
            accepted = parse_transition(content);
        }
        if (!accepted) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header) return {};
    if (out.empty()) {
        if (!push_guarded("cynefin-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("cynefin-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
