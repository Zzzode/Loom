module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A radar-beta chart declares axes, a set of curves (inline numeric arrays
// or a braced, possibly multi-line named block) and a few scalar display
// directives. The terminal has no polygon geometry, radial scale or color
// contract, so the projection is a bounded native outline: each statement is
// shape-validated and the physical lines are transcribed verbatim,
// preserving the source indentation. No geometry, no color.

constexpr int kRadarMaxStatements = 256;
constexpr int kRadarMaxRows = 1024;
constexpr int kRadarMaxTextBytes = 512;
constexpr int kRadarIndent = 2;

bool radar_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view radar_body(std::string_view source) {
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
bool is_ident(std::string_view token) {
    if (token.empty() || token.size() > static_cast<std::size_t>(kRadarMaxTextBytes))
        return false;
    if (!is_ident_start(token.front())) return false;
    return std::ranges::all_of(token, is_ident_char);
}
bool is_uint(std::string_view token) {
    return !token.empty() && std::ranges::all_of(token, [](char c) {
        return c >= '0' && c <= '9';
    });
}

// Printable, single-line ASCII prose (titles and quoted labels). Control
// bytes, DEL and non-ASCII are rejected rather than emitted as invisible
// cells.
bool is_prose(std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(kRadarMaxTextBytes))
        return false;
    return std::ranges::all_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u >= 0x20 && u <= 0x7E;
    });
}

// Consume an optional ["label"] alias after an identifier; returns true when
// no alias is present or a well-formed printable-ASCII label is consumed.
bool consume_optional_label(std::string_view& rest) {
    rest = trim_spaces(rest);
    if (!rest.starts_with('[')) return true;
    if (rest.size() < 2 || rest[1] != '"') return false;
    const auto close_quote = rest.find('"', 2);
    if (close_quote == std::string_view::npos) return false;
    if (!is_prose(rest.substr(2, close_quote - 2))) return false;
    if (close_quote + 1 >= rest.size() || rest[close_quote + 1] != ']') return false;
    rest = trim_spaces(rest.substr(close_quote + 2));
    return true;
}

// One curve value element: a positional unsigned integer ("3") or a named
// "ident uint" pair ("Stam 5").
bool is_value_element(std::string_view piece) {
    piece = trim_spaces(piece);
    if (piece.empty()) return false;
    if (is_uint(piece)) return true;
    const auto sp = piece.find(' ');
    if (sp == std::string_view::npos) return false;
    return is_ident(trim_spaces(piece.substr(0, sp))) &&
           is_uint(trim_spaces(piece.substr(sp + 1)));
}

// A comma-separated value list with at most one optional trailing comma. An
// empty slot is legal only as the final piece and only after a real element
// has been seen, so leading ("{,1}"), internal ("{1,,2}") and repeated
// ("{1,,}") commas all fail while the single trailing comma of a multi-line
// block ("Agility 2,") is accepted.
bool is_value_list(std::string_view list) {
    bool saw_element = false;
    std::string_view work = list;
    while (true) {
        const auto comma = work.find(',');
        const bool is_last = comma == std::string_view::npos;
        const std::string_view piece =
            trim_spaces(is_last ? work : work.substr(0, comma));
        if (piece.empty())
            return is_last && saw_element;  // exactly one trailing comma
        if (!is_value_element(piece)) return false;
        saw_element = true;
        if (is_last) return true;
        work = work.substr(comma + 1);
    }
}

// axis <spec>(, <spec>)* where a spec is an ident or an ident["label"].
bool is_axis(std::string_view content) {
    if (!content.starts_with("axis ")) return false;
    std::string_view rest = content.substr(5);
    bool saw = false;
    while (true) {
        const auto comma = rest.find(',');
        std::string_view tail =
            comma == std::string_view::npos ? rest : rest.substr(0, comma);
        const auto sp = tail.find_first_not_of(' ');
        if (sp == std::string_view::npos) return false;
        std::string_view spec_work = trim_spaces(tail);
        const auto id_end = spec_work.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
        std::string_view ident =
            id_end == std::string_view::npos ? spec_work : spec_work.substr(0, id_end);
        if (!is_ident(ident)) return false;
        std::string_view after = spec_work.substr(ident.size());
        if (!consume_optional_label(after) || !after.empty()) return false;
        saw = true;
        if (comma == std::string_view::npos) break;
        rest = rest.substr(comma + 1);
    }
    return saw;
}

// A curve header up to and including the opening brace. Sets block_open when
// the brace ends the line (a multi-line block); otherwise the inline value
// list and closing brace must be on the same line.
bool parse_curve_header(std::string_view content, bool& block_open) {
    block_open = false;
    if (!content.starts_with("curve ")) return false;
    std::string_view rest = trim_spaces(content.substr(6));
    const auto id_end = rest.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
    if (id_end == std::string_view::npos) return false;
    const std::string_view name = rest.substr(0, id_end);
    if (!is_ident(name)) return false;
    rest = trim_spaces(rest.substr(id_end));
    if (!consume_optional_label(rest)) return false;
    if (!rest.starts_with('{')) return false;
    rest = trim_spaces(rest.substr(1));
    if (rest.empty()) {
        block_open = true;  // multi-line block
        return true;
    }
    if (!rest.ends_with('}')) return false;
    const std::string_view list = rest.substr(0, rest.size() - 1);
    return is_value_list(list);
}

bool is_scalar_directive(std::string_view content) {
    auto two = [&](std::string_view keyword, bool (*arg_ok)(std::string_view)) {
        if (!content.starts_with(keyword)) return false;
        if (content.size() <= keyword.size() || content[keyword.size()] != ' ')
            return false;
        return arg_ok(trim_spaces(content.substr(keyword.size() + 1)));
    };
    if (two("ticks", is_uint) || two("max", is_uint) || two("min", is_uint))
        return true;
    if (two("showLegend", [](std::string_view v) { return v == "true" || v == "false"; }))
        return true;
    if (content.starts_with("graticule "))
        return trim_spaces(content.substr(10)) == "polygon";
    return false;
}

}  // namespace

std::optional<std::vector<std::string>> radar_layout(std::string_view source,
                                                     Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!radar_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = radar_body(source);
    // No polygon/color contract: a frontmatter theme/look/config key, an
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
    bool block_has_value = false;
    int statement_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kRadarMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
            if (first_non_space != 0 || content != "radar-beta") return {};
            saw_header = true;
            continue;
        }

        if (block_open) {
            // Inside a curve block: a deeper-indented value line, or the
            // column-2 closing brace. Nothing else may appear, and a block
            // must carry at least one value line before it can close.
            if (first_non_space == kRadarIndent && content == "}") {
                if (!block_has_value) return {};
                block_open = false;
            } else if (first_non_space > kRadarIndent) {
                if (!is_value_list(content)) return {};
                block_has_value = true;
            } else {
                return {};
            }
            if (!push_guarded(std::string{line})) return {};
            continue;
        }

        // A top-level statement sits on the strict two-space grid.
        if (first_non_space != kRadarIndent) return {};
        if (++statement_count > kRadarMaxStatements) return {};

        bool opens_block = false;
        bool accepted = false;
        if (content.starts_with("curve ")) {
            accepted = parse_curve_header(content, opens_block);
        } else if (content.starts_with("axis ")) {
            accepted = is_axis(content);
        } else if (content.starts_with("title ")) {
            accepted = is_prose(content.substr(6));
        } else {
            accepted = is_scalar_directive(content);
        }
        if (!accepted) return {};
        if (opens_block) {
            block_open = true;
            block_has_value = false;
        }
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header || block_open) return {};  // missing header or unclosed block
    if (out.empty()) {
        if (!push_guarded("radar-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("radar-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
