module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A wardley-beta map is a strict two-space indented frame of anchors and
// components positioned by normalized coordinates, value-chain and flow
// links, evolve/note/annotation directives, evolution-axis renames and
// pipeline blocks. The terminal has no map canvas, coordinate geometry or
// color contract, so the projection is a bounded native outline that
// validates each statement shape and transcribes the line verbatim,
// preserving its source indentation. No geometry, no color.

constexpr int kWardleyMaxStatements = 512;
constexpr int kWardleyMaxTextBytes = 512;
constexpr int kWardleyMaxRows = 1024;
constexpr int kWardleyIndent = 2;

bool wardley_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view wardley_body(std::string_view source) {
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

bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }

// An unsigned integer: one or more digits.
bool is_uint(std::string_view token) {
    return !token.empty() && std::ranges::all_of(token, is_ascii_digit);
}

// A signed integer: an optional single leading minus then digits. Label
// offsets may be negative.
bool is_int(std::string_view token) {
    if (token.empty()) return false;
    if (token.starts_with('-')) token.remove_prefix(1);
    return is_uint(token);
}

// A normalized coordinate decimal: digits with exactly one dot, at least one
// digit on each side (the corpus uses 0.x forms).
bool is_float(std::string_view token) {
    const auto dot = token.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot == token.size() - 1)
        return false;
    if (token.find('.', dot + 1) != std::string_view::npos) return false;
    return std::ranges::all_of(token, [](char c) { return is_ascii_digit(c) || c == '.'; });
}

// A component/anchor/pipeline name: ASCII letters, digits, spaces and
// balanced parentheses (the corpus uses names such as
// "byte pair encoding (BPE)"). No structural character may appear.
bool is_name(std::string_view name) {
    if (name.empty() || name.size() > static_cast<std::size_t>(kWardleyMaxTextBytes))
        return false;
    int depth = 0;
    for (char c : name) {
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '(' || c == ')';
        if (!allowed) return false;
        if (c == '(') ++depth;
        if (c == ')') {
            if (depth == 0) return false;
            --depth;
        }
    }
    return depth == 0 && trim_spaces(name).size() == name.size() && !name.empty();
}

// Prose inside a title, note or annotation: bounded ASCII text excluding the
// structural bracket/brace/quote/semicolon markers.
bool is_prose(std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(kWardleyMaxTextBytes))
        return false;
    for (char c : text) {
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == ',' || c == ':' ||
            c == '.' || c == '\'' || c == '(' || c == ')' || c == '-' || c == '/';
        if (!allowed) return false;
    }
    return true;
}

// A bracketed token: if content starts with '[', return the interior up to
// the next ']' and the remainder after it.
bool take_bracket(std::string_view& rest, std::string_view& interior) {
    if (!rest.starts_with('[')) return false;
    const auto close = rest.find(']');
    if (close == std::string_view::npos) return false;
    interior = rest.substr(1, close - 1);
    rest = rest.substr(close + 1);
    return true;
}

bool is_float_pair(std::string_view inner) {
    const auto comma = inner.find(',');
    if (comma == std::string_view::npos) return false;
    const auto left = trim_spaces(inner.substr(0, comma));
    const auto right = trim_spaces(inner.substr(comma + 1));
    // The corpus writes exactly one space after the comma; trim guards both
    // sides while requiring the single-comma shape.
    if (inner.find(',', comma + 1) != std::string_view::npos) return false;
    return is_float(left) && is_float(right);
}

bool is_int_pair(std::string_view inner) {
    const auto comma = inner.find(',');
    if (comma == std::string_view::npos) return false;
    if (inner.find(',', comma + 1) != std::string_view::npos) return false;
    return is_int(trim_spaces(inner.substr(0, comma))) &&
           is_int(trim_spaces(inner.substr(comma + 1)));
}

// An unsigned pair: canvas sizes and annotation grid indices never carry a
// sign (only label offsets may be negative).
bool is_uint_pair(std::string_view inner) {
    const auto comma = inner.find(',');
    if (comma == std::string_view::npos) return false;
    if (inner.find(',', comma + 1) != std::string_view::npos) return false;
    return is_uint(trim_spaces(inner.substr(0, comma))) &&
           is_uint(trim_spaces(inner.substr(comma + 1)));
}

// size [int, int]
bool is_size(std::string_view content) {
    if (!content.starts_with("size ")) return false;
    std::string_view rest = trim_spaces(content.substr(5));
    std::string_view inner;
    return take_bracket(rest, inner) && rest.empty() && is_uint_pair(inner);
}

// A node statement (anchor/component/deaccelerator): keyword, a name, then a
// float-pair coordinate (a pipeline component carries a single float) and an
// optional " label [int, int]" suffix.
bool is_node(std::string_view content, std::string_view keyword, bool single_coord) {
    if (!content.starts_with(keyword)) return false;
    std::string_view rest = content.substr(keyword.size());
    const auto bracket = rest.find('[');
    if (bracket == std::string_view::npos) return false;
    if (!is_name(trim_spaces(rest.substr(0, bracket)))) return false;
    rest = rest.substr(bracket);
    std::string_view coord;
    if (!take_bracket(rest, coord)) return false;
    const bool coord_ok = single_coord ? is_float(trim_spaces(coord))
                                       : is_float_pair(coord);
    if (!coord_ok) return false;
    rest = trim_spaces(rest);
    if (rest.empty()) return true;
    if (!rest.starts_with("label ")) return false;
    rest = trim_spaces(rest.substr(6));
    std::string_view offset;
    return take_bracket(rest, offset) && rest.empty() && is_int_pair(offset);
}

// evolve <name> <float>
bool is_evolve(std::string_view content) {
    if (!content.starts_with("evolve ")) return false;
    const auto rest = trim_spaces(content.substr(7));
    const auto sp = rest.rfind(' ');
    if (sp == std::string_view::npos) return false;
    return is_name(trim_spaces(rest.substr(0, sp))) &&
           is_float(trim_spaces(rest.substr(sp + 1)));
}

// note "<prose>" [float, float]
bool is_note(std::string_view content) {
    if (!content.starts_with("note ")) return false;
    std::string_view rest = trim_spaces(content.substr(5));
    if (!rest.starts_with('"')) return false;
    const auto close = rest.find('"', 1);
    if (close == std::string_view::npos) return false;
    if (!is_prose(rest.substr(1, close - 1))) return false;
    rest = trim_spaces(rest.substr(close + 1));
    std::string_view coord;
    return take_bracket(rest, coord) && rest.empty() && is_float_pair(coord);
}

// evolution <name> -> <name> -> ...
bool is_evolution(std::string_view content) {
    if (!content.starts_with("evolution ")) return false;
    std::string_view rest = content.substr(10);
    int segments = 0;
    while (true) {
        const auto arrow = rest.find(" -> ");
        const std::string_view token =
            arrow == std::string_view::npos ? rest : rest.substr(0, arrow);
        if (!is_name(trim_spaces(token))) return false;
        ++segments;
        if (arrow == std::string_view::npos) break;
        rest = rest.substr(arrow + 4);
    }
    return segments >= 2;
}

// pipeline <name> {
bool is_pipeline_open(std::string_view content, std::string_view& name) {
    if (!content.starts_with("pipeline ") || !content.ends_with('{')) return false;
    name = trim_spaces(content.substr(9, content.size() - 10));
    return is_name(name);
}

// deaccelerator <name> [float, float]
bool is_deaccelerator(std::string_view content) {
    return is_node(content, "deaccelerator ", /*single_coord=*/false);
}

// annotations [int, int]
bool is_annotations_header(std::string_view content) {
    if (!content.starts_with("annotations ")) return false;
    std::string_view rest = trim_spaces(content.substr(12));
    std::string_view inner;
    return take_bracket(rest, inner) && rest.empty() && is_uint_pair(inner);
}

// annotation <uint>,[float, float] "<prose>"  (no space after the comma)
bool is_annotation(std::string_view content) {
    if (!content.starts_with("annotation ")) return false;
    std::string_view rest = trim_spaces(content.substr(11));
    const auto comma = rest.find(',');
    if (comma == std::string_view::npos ||
        !is_uint(trim_spaces(rest.substr(0, comma))))
        return false;
    rest = rest.substr(comma + 1);
    std::string_view coord;
    if (!rest.starts_with('[') || !take_bracket(rest, coord) ||
        !is_float_pair(coord))
        return false;
    rest = trim_spaces(rest);
    if (!rest.starts_with('"') || !rest.ends_with('"') || rest.size() < 2)
        return false;
    return is_prose(rest.substr(1, rest.size() - 2));
}

// A link head split around one operator into two validated names.
bool split_link(std::string_view head, std::string_view op,
                std::string_view& left, std::string_view& right) {
    const auto pos = head.find(op);
    if (pos == std::string_view::npos) return false;
    if (head.find(op, pos + op.size()) != std::string_view::npos) return false;
    left = trim_spaces(head.substr(0, pos));
    right = trim_spaces(head.substr(pos + op.size()));
    return is_name(left) && is_name(right);
}

// A link line: <name> <op> <name> with an optional "; continuation"
// (constraint keyword or a bare flow operator). Operators: ->, +>, +<>, +<,
// and the custom-labeled +'label'> form.
bool is_link(std::string_view content) {
    std::string_view head = content;
    const auto semi = content.find(';');
    if (semi != std::string_view::npos) {
        head = content.substr(0, semi);
        const auto tail = trim_spaces(content.substr(semi + 1));
        if (tail != "constraint" && tail != "+>" && tail != "+<>" &&
            tail != "+<")
            return false;
    }
    head = trim_spaces(head);

    // Custom-labeled flow: <name> +'label'> <name>
    if (const auto label_open = head.find("+'");
        label_open != std::string_view::npos) {
        const auto label_close = head.find("'>", label_open + 2);
        if (label_close == std::string_view::npos) return false;
        const auto label = head.substr(label_open + 2,
                                       label_close - label_open - 2);
        if (label.empty() || !is_prose(label)) return false;
        const auto left = trim_spaces(head.substr(0, label_open));
        const auto right = trim_spaces(head.substr(label_close + 2));
        return is_name(left) && is_name(right);
    }

    std::string_view left;
    std::string_view right;
    // Order matters: the three-char +<> before the two-char +< / +>.
    if (split_link(head, "+<>", left, right)) return true;
    if (split_link(head, "+>", left, right)) return true;
    if (split_link(head, "+<", left, right)) return true;
    return split_link(head, "->", left, right);
}

// D-437: the closed frontmatter theme. `base`/`dark`/`forest`/`neutral` are
// colour palettes the terminal projection never draws, so the leaf is inert and
// disclosed under `Wardley native projection v1`. Tri-state: `accepted`
// separates "no theme" (plain source or description-only) from "not an admitted
// frontmatter".
struct WardleyTheme {
    bool accepted = false;
    std::optional<std::string> theme;
};

WardleyTheme wardley_frontmatter_theme(std::string_view source) {
    WardleyTheme result;
    if (!source.starts_with("---\n")) {
        result.accepted = true;  // a plain source carries no frontmatter
        return result;
    }
    std::size_t close_start = 0;
    for (std::size_t search = 4;;) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                               : nl - search);
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            std::size_t i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i == line.size()) {
                if (nl == std::string_view::npos) return result;  // unterminated
                close_start = search;
                break;
            }
        }
        if (nl == std::string_view::npos) return result;
        search = nl + 1;
    }
    const std::string_view frontmatter = source.substr(0, close_start);
    bool description_seen = false, config_seen = false, theme_seen = false, first = true;
    for (std::size_t pos = 0; pos <= frontmatter.size();) {
        const auto nl = frontmatter.find('\n', pos);
        const std::string_view line =
            frontmatter.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                                 : nl - pos);
        const bool last = nl == std::string_view::npos;
        pos = last ? frontmatter.size() + 1u : nl + 1u;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {
            if (last) break;
            continue;
        }
        const std::string_view content = line.substr(key_start);
        if (first) {
            if (key_start != 0u || content != "---") return result;
            first = false;
            if (last) break;
            continue;
        }
        if (key_start == 0u) {
            if (content.starts_with("description:")) {
                if (description_seen || config_seen) return result;
                description_seen = true;
            } else if (content == "config:") {
                if (config_seen || !description_seen) return result;
                config_seen = true;
            } else {
                return result;  // a top-level theme/look key stays unadmitted
            }
            if (last) break;
            continue;
        }
        if (!config_seen) {  // a description payload line
            if (!description_seen) return result;
            if (last) break;
            continue;
        }
        // The config block is closed: exactly one indent-2 `theme` leaf.
        if (theme_seen || key_start != 2u || !content.starts_with("theme: ")) return result;
        const std::string_view value = trim_spaces(content.substr(7u));
        if (value != "base" && value != "dark" && value != "forest" && value != "neutral")
            return result;
        theme_seen = true;
        result.theme = std::string{value};
        if (last) break;
    }
    if (config_seen && !theme_seen) return result;
    if (!description_seen) return result;
    result.accepted = true;
    return result;
}

}  // namespace

std::optional<std::vector<std::string>> wardley_layout(std::string_view source,
                                                       Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!wardley_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = wardley_body(source);
    // D-437: the closed frontmatter parser owns the admitted shape; an
    // in-body %%{init}%% directive or a classDef/:::/style statement keeps the
    // whole source on fallback. Key-based so prose mentioning "theme" is not a
    // gate.
    const WardleyTheme frontmatter = wardley_frontmatter_theme(source);
    if (!frontmatter.accepted) return {};
    {
        bool visual_gate = false;
        {
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
    int pipeline_depth = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kWardleyMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
            if (content != "wardley-beta") return {};
            saw_header = true;
            continue;
        }
        // D-437: the corpus spells whole maps at column 0 (upstream style) and
        // earlier slices at the two-space grid; both are accepted, and the
        // source indentation is preserved verbatim in the emitted rows.
        if (first_non_space != 0u && first_non_space != static_cast<std::size_t>(kWardleyIndent))
            return {};
        if (++statement_count > kWardleyMaxStatements) return {};

        std::string_view pipeline_name;
        bool accepted = false;
        if (content == "}") {
            // A pipeline block closes; only valid when one is open and only
            // component lines were accepted inside it.
            if (pipeline_depth != 1) return {};
            pipeline_depth = 0;
            accepted = true;
        } else if (is_pipeline_open(content, pipeline_name)) {
            if (pipeline_depth != 0) return {};  // blocks do not nest
            pipeline_depth = 1;
            accepted = true;
        } else if (pipeline_depth == 1) {
            // Inside a pipeline only single-coordinate components are legal.
            accepted = is_node(content, "component ", /*single_coord=*/true);
        } else if (content.starts_with("component ")) {
            accepted = is_node(content, "component ", /*single_coord=*/false);
        } else if (content.starts_with("anchor ")) {
            accepted = is_node(content, "anchor ", /*single_coord=*/false);
        } else if (content.starts_with("deaccelerator ")) {
            accepted = is_deaccelerator(content);
        } else if (content.starts_with("evolve ")) {
            accepted = is_evolve(content);
        } else if (content.starts_with("note ")) {
            accepted = is_note(content);
        } else if (content.starts_with("annotation ")) {
            accepted = is_annotation(content);
        } else if (content.starts_with("annotations ")) {
            accepted = is_annotations_header(content);
        } else if (content.starts_with("evolution ")) {
            accepted = is_evolution(content);
        } else if (content.starts_with("title ")) {
            accepted = is_prose(content.substr(6));
        } else if (content.starts_with("size ")) {
            accepted = is_size(content);
        } else if (content.find("->") != std::string_view::npos ||
                   content.find('+') != std::string_view::npos) {
            accepted = is_link(content);
        }
        if (!accepted) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header) return {};
    if (pipeline_depth != 0) return {};  // unbalanced pipeline block
    if (out.empty()) {
        if (!push_guarded("wardley-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("wardley-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    if (frontmatter.theme) {
        const std::string line = "config.theme=" + *frontmatter.theme +
                                 " -> colour palette; not applied";
        if (line.size() > 96u || !work.spend(line.size() * 4u + 128u)) return {};
        if (out.size() + 2u > static_cast<std::size_t>(kWardleyMaxRows)) return {};
        out.push_back("");
        out.push_back("Wardley native projection v1");
        out.push_back(line);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
